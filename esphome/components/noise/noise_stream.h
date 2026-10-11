#pragma once

#include "esphome/core/defines.h"

#ifdef USE_NOISE_STREAM

#include "noise.h"
#include "noise_handshake.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::noise {

/** Noise_NNpsk0 session for a byte stream between two devices that hold the same key.
 *
 * Frames use the api wire format: an indicator byte, a 16-bit length, then the payload. The initiator
 * speaks first. The stream knows no transport: the methods taking a Link drive any type with
 * TcpClientLink's calls (connected, read, tx_free, tx_tail, tx_commit, flush_tx, close, note_attempt,
 * note_io, check_idle, reconnect_interval).
 * Plaintext written between two flush() calls is encrypted in place in the link's buffer as one frame,
 * so the stream keeps only a receive buffer.
 */
class NoiseStream {
 public:
  NoiseStream(const uint8_t *psk, bool initiator) : initiator_(initiator) { this->ctx_.set_psk(psk); }
  NoiseStream(const NoiseStream &) = delete;
  NoiseStream &operator=(const NoiseStream &) = delete;

  /// tag names the owner's log lines.
  void set_log_tag(const char *tag) { this->tag_ = tag; }
  bool ready() const { return this->state_ == State::STATE_READY; }

  /// True once the session is secure. While the link is up this runs the handshake, and a failed or
  /// stalled handshake closes the link; so does a connection made while an initiator holds its next
  /// attempt. A down link resets the session. Call it every loop, also while the link is down.
  template<typename Link> bool up(Link &link) {
    if (!link.connected()) {
      this->idle_(link);
      return false;
    }
    if (this->state_ == State::STATE_READY) {
      return true;
    }
    return this->up_slow_(link);
  }

  /// Plaintext out of the link. Same returns as TcpClientLink::read(); a frame that fails to decrypt
  /// closes the link.
  template<typename Link> ssize_t read(Link &link, uint8_t *buf, size_t len) {
    size_t got = this->take_plain_(buf, len);
    while (got < len && this->state_ == State::STATE_READY) {
      size_t want = 0;
      uint8_t *space = this->rx_space_(want);
      ssize_t count = link.read(space, want);
      if (count <= 0) {
        if (count < 0) {
          this->reset();
          return got != 0 ? static_cast<ssize_t>(got) : -1;
        }
        break;
      }
      const LogString *error = this->rx_commit_(static_cast<size_t>(count));
      if (error != nullptr) {
        this->fail_(link, error, nullptr);
        return got != 0 ? static_cast<ssize_t>(got) : -1;
      }
      got += this->take_plain_(buf + got, len - got);
    }
    if (got != 0) {
      // Plaintext taken from a frame read earlier involves no socket read, so note it for the idle timeout
      link.note_io();
    }
    return static_cast<ssize_t>(got);
  }

  /// Plaintext into the open frame; returns how many bytes fit.
  template<typename Link> size_t queue(Link &link, const uint8_t *data, size_t len) {
    if (this->state_ != State::STATE_READY || !link.connected()) {
      return 0;
    }
    size_t taken = 0;
    while (taken < len) {
      if (this->open_len_ == 0) {
        if (link.tx_free() <= FRAME_OVERHEAD) {
          break;
        }
        // The header is written when the frame is sealed
        link.tx_commit(FRAME_HEADER_SIZE);
      }
      size_t free = link.tx_free();
      if (free <= MAC_SIZE) {
        break;
      }
      size_t count = std::min({len - taken, MAX_PLAIN - this->open_len_, free - MAC_SIZE});
      std::memcpy(link.tx_tail(), data + taken, count);
      link.tx_commit(count);
      this->open_len_ += static_cast<uint16_t>(count);
      taken += count;
      if (this->open_len_ == MAX_PLAIN && !this->seal_(link)) {
        break;
      }
    }
    return taken;
  }

  /// The plaintext room queue() grants in one call.
  template<typename Link> size_t tx_free(const Link &link) const {
    if (this->state_ != State::STATE_READY) {
      return 0;
    }
    size_t free = link.tx_free();
    if (this->open_len_ != 0) {
      return free > MAC_SIZE ? std::min(MAX_PLAIN - this->open_len_, free - MAC_SIZE) : 0;
    }
    return free > FRAME_OVERHEAD ? std::min(MAX_PLAIN, free - FRAME_OVERHEAD) : 0;
  }

  /// Seal the open frame and send; true once the link's buffer is empty.
  template<typename Link> bool flush(Link &link) {
    if (this->open_len_ != 0) {
      // Closing the link dropped the open frame with its buffer
      if (!link.connected()) {
        this->reset();
        return false;
      }
      if (!this->seal_(link)) {
        return false;
      }
    }
    return link.flush_tx();
  }

  /// Forget the session. up() and flush() do this when they find the link closed; an owner calls it only when
  /// it closes the link and may reopen it before the next up().
  void reset();

 protected:
  enum class State : uint8_t {
    STATE_IDLE,
    STATE_HANDSHAKE,
    STATE_READY,
  };
  enum class Step : uint8_t {
    STEP_WRITE,
    STEP_READ,
    STEP_DONE,
    STEP_FAILED,
  };

  // Two full frames fit a 1 KiB link buffer; a longer write is split.
  static constexpr size_t MAX_PLAIN = 480;
  static constexpr size_t FRAME_OVERHEAD = FRAME_HEADER_SIZE + MAC_SIZE;
  static constexpr size_t HANDSHAKE_FRAME_SIZE = FRAME_HEADER_SIZE + 1 + MAX_HANDSHAKE_SIZE;
  static constexpr size_t RX_SIZE = FRAME_HEADER_SIZE + MAX_PLAIN + MAC_SIZE;
  static_assert(RX_SIZE >= HANDSHAKE_FRAME_SIZE, "a handshake frame must fit the receive buffer");
  // Longest wait between an initiator's failed handshakes, as the api's outgoing connection
  static constexpr uint32_t BACKOFF_MAX_MS = 300000;

  template<typename Link> bool up_slow_(Link &link) {
    if (this->state_ == State::STATE_IDLE) {
      if (this->holding_()) {
        // A loop pass longer than the link's interval let it connect before the hold ran out
        link.close();
        link.note_attempt();
        return false;
      }
      const LogString *error = this->start_();
      if (error != nullptr) {
        this->fail_(link, error, nullptr);
        return false;
      }
    } else if (this->timed_out_()) {
      this->fail_(link, LOG_STR("Handshake timed out"), nullptr);
      return false;
    }
    for (;;) {
      switch (this->next_step_()) {
        case Step::STEP_WRITE: {
          if (link.tx_free() < HANDSHAKE_FRAME_SIZE) {
            link.flush_tx();
            return false;
          }
          size_t frame = this->write_handshake_(link.tx_tail());
          if (frame == 0) {
            this->fail_(link, LOG_STR("Handshake write failed"), nullptr);
            return false;
          }
          link.tx_commit(frame);
          link.flush_tx();
          break;
        }
        case Step::STEP_READ: {
          // What the last write left in the buffer goes out first
          link.flush_tx();
          size_t want = 0;
          uint8_t *space = this->rx_space_(want);
          ssize_t count = link.read(space, want);
          if (count <= 0) {
            // The link's idle timeout also holds while the handshake waits for the peer
            link.check_idle();
            if (!link.connected()) {
              this->abort_(link.reconnect_interval());
            }
            return false;
          }
          const LogString *reject = nullptr;
          const LogString *error = this->rx_commit_(static_cast<size_t>(count), &reject);
          if (error != nullptr) {
            this->fail_(link, error, reject);
            return false;
          }
          break;
        }
        case Step::STEP_DONE:
          return link.connected();
        default:
          this->fail_(link, LOG_STR("Handshake failed"), nullptr);
          return false;
      }
      if (!link.connected()) {
        this->abort_(link.reconnect_interval());
        return false;
      }
    }
  }

  /// Log, send a responder's reject reason when there is one, close the link and abort.
  template<typename Link> void fail_(Link &link, const LogString *error, const LogString *reject) {
    this->log_failure_(error);
    if (reject != nullptr && !this->initiator_ && link.tx_free() >= HANDSHAKE_FRAME_SIZE) {
      link.tx_commit(this->write_reject_(link.tx_tail(), reject));
      link.flush_tx();
    }
    link.close();
    link.note_attempt();
    this->abort_(link.reconnect_interval());
  }

  /// Encrypt the open frame in place at the link's tail and append its MAC.
  template<typename Link> bool seal_(Link &link) {
    uint8_t *frame = link.tx_tail() - this->open_len_ - FRAME_HEADER_SIZE;
    if (!this->seal_frame_(frame)) {
      this->fail_(link, LOG_STR("Encrypt failed"), nullptr);
      return false;
    }
    link.tx_commit(MAC_SIZE);
    return true;
  }

  /// No link: reset, hold an initiator's next attempt after a failed handshake, and without the api a
  /// responder prepares the spare key for its next peer.
  template<typename Link> void idle_(Link &link) {
    if (this->state_ != State::STATE_IDLE) {
      this->reset();
    }
    if (this->hold_ms_ != 0) {
      // The link retries one interval after its last attempt, so the hold keeps that clock at now
      if (this->holding_()) {
        link.note_attempt();
      } else {
        this->hold_ms_ = 0;
      }
    }
#if defined(USE_NOISE_SPARE_EPHEMERAL) && !defined(USE_API)
    // With the api, APIServer::loop refills it under its connect grace
    if (!this->initiator_ && !has_spare_ephemeral()) {
      prepare_spare_ephemeral();
    }
#endif
  }
  /// An initiator's hold after a failed handshake has not run out.
  bool holding_() const { return App.get_loop_component_start_time() - this->started_ms_ < this->hold_ms_; }
  const LogString *start_();
  bool timed_out_() const;
  /// The handshake ended without a session: reset, and an initiator holds its next attempt for the next
  /// retry wait. interval is the link's own wait, part of the hold and the shortest one.
  void abort_(uint32_t interval);
  /// The wait after one more failed handshake: the backoff with +/-20% jitter, then the backoff doubles.
  uint32_t next_retry_wait_(uint32_t interval) {
    // The rule of the api's outgoing connection, starting at the link's interval
    uint32_t backoff = std::max(this->backoff_ms_, interval);
    uint32_t jitter_span = backoff / 5;
    this->backoff_ms_ = std::min(backoff * 2, BACKOFF_MAX_MS);
    return backoff - jitter_span + (random_uint32() % (2 * jitter_span + 1));
  }
  Step next_step_();
  /// Frame the next handshake message at out (room for HANDSHAKE_FRAME_SIZE); 0 on error.
  size_t write_handshake_(uint8_t *out);
  size_t write_reject_(uint8_t *out, const LogString *reason);
  /// Where the next received bytes go and how many the current frame still needs.
  uint8_t *rx_space_(size_t &want);
  /// Count bytes landed in rx_space_(). A complete frame is processed; non-null is the failure.
  const LogString *rx_commit_(size_t count, const LogString **reject = nullptr);
  size_t take_plain_(uint8_t *buf, size_t len);
  bool seal_frame_(uint8_t *frame);
  void log_failure_(const LogString *error);

  NoiseContext ctx_;
  NoiseResponderHandshake handshake_;
  NoiseCipherState *send_{nullptr};
  NoiseCipherState *recv_{nullptr};
  const char *tag_{"noise"};
  // Start of the handshake, or of an initiator's retry hold after a failed one.
  uint32_t started_ms_{0};
  // Hold time past the link's own interval; 0 when no hold runs.
  uint32_t hold_ms_{0};
  // Backoff for the next failed handshake; 0 starts at the link's interval.
  uint32_t backoff_ms_{0};
  // rx_ holds the frame being read, then its plaintext at [plain_off_, plain_end_).
  uint16_t rx_have_{0};
  uint16_t rx_need_{0};
  uint16_t plain_off_{0};
  uint16_t plain_end_{0};
  // Plaintext bytes of the open frame at the link's tail; 0 means no frame is open.
  uint16_t open_len_{0};
  State state_{State::STATE_IDLE};
  bool initiator_;
  uint8_t rx_[RX_SIZE]{};
};

}  // namespace esphome::noise

#endif  // USE_NOISE_STREAM
