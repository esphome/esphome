#include "noise_stream.h"

#ifdef USE_NOISE_STREAM

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"

#include <algorithm>

namespace esphome::noise {

// Counted from the connection, as the api's: ESP8266 light sleep on weak WiFi stretches a handshake to 28-30 s.
static constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 60000;

static constexpr char PROLOGUE[] PROGMEM = "NoiseStreamInit";
static constexpr size_t PROLOGUE_LEN = sizeof(PROLOGUE) - 1;

// start_() and rx_commit_() return this after they logged the cause, so log_failure_() skips it
static constexpr char ALREADY_LOGGED[] PROGMEM = "";

// Stream version in the Hello, apart from the ESPHome release. Every peer version is accepted; a later minor
// version may add fields after these two bytes, which an older peer ignores.
static constexpr uint8_t VERSION_MAJOR = 1;
static constexpr uint8_t VERSION_MINOR = 0;
static constexpr size_t HELLO_SIZE = 2;

void NoiseStream::reset() {
  this->handshake_.release();
  if (this->send_ != nullptr) {
    noise_cipherstate_free(this->send_);
    this->send_ = nullptr;
  }
  if (this->recv_ != nullptr) {
    noise_cipherstate_free(this->recv_);
    this->recv_ = nullptr;
  }
  this->rx_have_ = 0;
  this->rx_need_ = 0;
  this->plain_off_ = 0;
  this->plain_end_ = 0;
  this->open_len_ = 0;
  this->state_ = State::STATE_IDLE;
}

const LogString *NoiseStream::start_() {
  uint8_t prologue[PROLOGUE_LEN];
  progmem_memcpy(prologue, PROLOGUE, PROLOGUE_LEN);
  int err = this->initiator_ ? this->handshake_.init_initiator(this->ctx_, prologue, PROLOGUE_LEN)
                             : this->handshake_.init(this->ctx_, prologue, PROLOGUE_LEN);
  if (err != 0) {
    ESP_LOGW(this->tag_, "Handshake init failed: %s", LOG_STR_ARG(noise_err_to_logstr(err)));
    return reinterpret_cast<const LogString *>(ALREADY_LOGGED);
  }
  this->state_ = State::STATE_HANDSHAKE;
  this->started_ms_ = App.get_loop_component_start_time();
  // started_ms_ now dates the handshake, so a hold that ran out must not count from it after a later drop
  this->hold_ms_ = 0;
  return nullptr;
}

bool NoiseStream::timed_out_() const {
  return App.get_loop_component_start_time() - this->started_ms_ >= HANDSHAKE_TIMEOUT_MS;
}

void NoiseStream::abort_(uint32_t interval) {
  this->reset();
  if (this->initiator_) {
    uint32_t wait = this->next_retry_wait_(interval);
    this->started_ms_ = App.get_loop_component_start_time();
    this->hold_ms_ = wait > interval ? wait - interval : 0;
  }
}

NoiseStream::Step NoiseStream::next_step_() {
  switch (this->state_) {
    case State::STATE_HELLO_SEND:
      return Step::STEP_WRITE;
    case State::STATE_HELLO_WAIT:
      return Step::STEP_READ;
    case State::STATE_READY:
      // Once per session: from now on up() returns before up_slow_()
      this->backoff_ms_ = 0;
      ESP_LOGD(this->tag_, "Session encrypted");
      return Step::STEP_DONE;
    default:
      break;
  }
  switch (this->handshake_.action()) {
    case NoiseResponderHandshake::Action::ACTION_WRITE:
      return Step::STEP_WRITE;
    case NoiseResponderHandshake::Action::ACTION_READ:
      return Step::STEP_READ;
    case NoiseResponderHandshake::Action::ACTION_SPLIT:
      if (this->handshake_.split(this->send_, this->recv_) != 0) {
        return Step::STEP_FAILED;
      }
      // The initiator's Hello goes first
      this->state_ = this->initiator_ ? State::STATE_HELLO_SEND : State::STATE_HELLO_WAIT;
      return this->initiator_ ? Step::STEP_WRITE : Step::STEP_READ;
    default:
      return Step::STEP_FAILED;
  }
}

size_t NoiseStream::write_handshake_(uint8_t *out) {
  if (this->state_ == State::STATE_HELLO_SEND) {
    out[FRAME_HEADER_SIZE] = VERSION_MAJOR;
    out[FRAME_HEADER_SIZE + 1] = VERSION_MINOR;
    if (!this->seal_frame_(out, HELLO_SIZE)) {
      return 0;
    }
    // The responder's Hello answers the initiator's
    this->state_ = this->initiator_ ? State::STATE_HELLO_WAIT : State::STATE_READY;
    return FRAME_OVERHEAD + HELLO_SIZE;
  }
  size_t len = 0;
  if (this->handshake_.write_message(out + FRAME_HEADER_SIZE + 1, MAX_HANDSHAKE_SIZE, len) != 0) {
    return 0;
  }
  out[FRAME_HEADER_SIZE] = HANDSHAKE_STATUS_OK;
  write_frame_header(out, static_cast<uint16_t>(len + 1));
  return FRAME_HEADER_SIZE + 1 + len;
}

size_t NoiseStream::write_reject_(uint8_t *out, const LogString *reason) {
  size_t len = format_reject_payload(out + FRAME_HEADER_SIZE, 1 + MAX_HANDSHAKE_SIZE, reason);
  write_frame_header(out, static_cast<uint16_t>(len));
  return FRAME_HEADER_SIZE + len;
}

uint8_t *NoiseStream::rx_space_(size_t &want) {
  if (this->rx_need_ == 0) {
    this->rx_need_ = FRAME_HEADER_SIZE;
  }
  want = this->rx_need_ - this->rx_have_;
  return this->rx_ + this->rx_have_;
}

const LogString *NoiseStream::rx_commit_(size_t count, const LogString **reject) {
  this->rx_have_ += static_cast<uint16_t>(count);
  if (this->rx_have_ < this->rx_need_) {
    return nullptr;
  }
  if (this->rx_need_ == FRAME_HEADER_SIZE) {
    size_t payload = (static_cast<size_t>(this->rx_[1]) << 8) | this->rx_[2];
    size_t limit = this->state_ > State::STATE_HANDSHAKE ? MAX_PLAIN + MAC_SIZE : 1 + MAX_HANDSHAKE_SIZE;
    if (this->rx_[0] != FRAME_INDICATOR || payload == 0 || payload > limit) {
      return LOG_STR("Bad frame");
    }
    this->rx_need_ = static_cast<uint16_t>(FRAME_HEADER_SIZE + payload);
    return nullptr;
  }
  uint8_t *payload = this->rx_ + FRAME_HEADER_SIZE;
  size_t payload_len = this->rx_need_ - FRAME_HEADER_SIZE;
  this->rx_have_ = 0;
  this->rx_need_ = 0;
  if (this->state_ > State::STATE_HANDSHAKE) {
    NoiseBuffer mbuf;
    noise_buffer_init(mbuf);
    noise_buffer_set_inout(mbuf, payload, payload_len, payload_len);
    if (noise_cipherstate_decrypt(this->recv_, &mbuf) != 0) {
      return LOG_STR("Decrypt failed");
    }
    if (this->state_ == State::STATE_HELLO_WAIT) {
      if (mbuf.size < HELLO_SIZE) {
        return LOG_STR("Bad hello");
      }
      ESP_LOGV(this->tag_, "Peer version %u.%u", payload[0], payload[1]);
      this->state_ = this->initiator_ ? State::STATE_READY : State::STATE_HELLO_SEND;
      return nullptr;
    }
    this->plain_off_ = FRAME_HEADER_SIZE;
    this->plain_end_ = static_cast<uint16_t>(FRAME_HEADER_SIZE + mbuf.size);
    return nullptr;
  }
  if (payload[0] != HANDSHAKE_STATUS_OK) {
    // Only a responder sends a reason; the bytes of a peer that dialed in are not logged
    if (!this->initiator_) {
      return LOG_STR("Bad handshake status");
    }
    const char *reason = reinterpret_cast<const char *>(payload + 1);
    // A wrong key arrives as the api's MAC failure text; name the key instead
    if (payload_len == MAC_FAILURE_PAYLOAD_SIZE &&
        ESPHOME_strncmp_P(reason, LOG_STR_ARG(reject_reason_for(NOISE_ERROR_MAC_FAILURE)), payload_len - 1) == 0) {
      return LOG_STR("Peer rejected the handshake: wrong encryption key");
    }
    ESP_LOGW(this->tag_, "Peer rejected the handshake: %.*s", static_cast<int>(payload_len - 1), reason);
    return reinterpret_cast<const LogString *>(ALREADY_LOGGED);
  }
  int err = this->handshake_.read_message(payload + 1, payload_len - 1);
  if (err != 0) {
    const LogString *reason = reject_reason_for(err);
    if (reject != nullptr) {
      *reject = reason;
    }
    return err == NOISE_ERROR_MAC_FAILURE ? LOG_STR("Handshake failed: wrong encryption key") : reason;
  }
  return nullptr;
}

size_t NoiseStream::take_plain_(uint8_t *buf, size_t len) {
  size_t count = std::min(len, static_cast<size_t>(this->plain_end_ - this->plain_off_));
  if (count != 0) {
    std::memcpy(buf, this->rx_ + this->plain_off_, count);
    this->plain_off_ += static_cast<uint16_t>(count);
  }
  return count;
}

bool NoiseStream::seal_frame_(uint8_t *frame, size_t len) {
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_inout(mbuf, frame + FRAME_HEADER_SIZE, len, len + MAC_SIZE);
  if (noise_cipherstate_encrypt(this->send_, &mbuf) != 0) {
    return false;
  }
  write_frame_header(frame, static_cast<uint16_t>(mbuf.size));
  return true;
}

void NoiseStream::log_failure_(const LogString *error) {
  if (error == reinterpret_cast<const LogString *>(ALREADY_LOGGED)) {
    return;
  }
  ESP_LOGW(this->tag_, "%s", LOG_STR_ARG(error));
}

void NoiseStream::log_reject_unsent_() { ESP_LOGV(this->tag_, "No room to send the reject reason"); }

}  // namespace esphome::noise

#endif  // USE_NOISE_STREAM
