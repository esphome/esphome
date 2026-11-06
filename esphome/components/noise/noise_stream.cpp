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
#ifdef USE_NOISE_STREAM_RUNTIME_KEY
  // Without a key the handshake would use the all-zeros key, which anyone holds
  if (!this->ctx_.has_psk()) {
    return LOG_STR("No encryption key");
  }
#endif
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

#ifdef USE_NOISE_STREAM_RUNTIME_KEY
bool NoiseStream::load_key() {
  if (this->ctx_.has_psk()) {
    return true;
  }
  this->pref_ = global_preferences->make_preference<psk_t>(this->key_hash_, true);
  this->loaded_ = true;
  psk_t saved;
  // A missing record, or the reserved all-zeros key, is no key
  if (!this->pref_.load(&saved) || NoiseContext::is_all_zeros(saved)) {
    ESP_LOGW(this->tag_, "No encryption key, the link stays closed until one is set");
    return false;
  }
  // The api or ota may have got this key since it was saved
  if (this->key_in_use_(saved)) {
    ESP_LOGW(this->tag_, "Saved key equals the api or ota key; the link stays closed until a new key is set");
    return false;
  }
  this->key_ = saved;
  this->ctx_.set_psk(this->key_.data());
  return true;
}

bool NoiseStream::save_key_(const std::string &key) {
  if (!this->loaded_) {
    // An on_boot action at or above the owner's setup priority runs before load_key()
    ESP_LOGW(this->tag_, "Encryption key set before setup; in on_boot use a priority below 200");
    return false;
  }
  // One byte more than a key, so a longer key is refused instead of cut
  uint8_t raw[sizeof(psk_t) + 1];
  psk_t psk;
  if (base64_decode(reinterpret_cast<const uint8_t *>(key.data()), key.size(), raw, sizeof(raw)) != psk.size()) {
    ESP_LOGW(this->tag_, "Invalid encryption key length");
    return false;
  }
  std::memcpy(psk.data(), raw, psk.size());
  if (NoiseContext::is_all_zeros(psk)) {
    ESP_LOGW(this->tag_, "Rejecting all-zero encryption key");
    return false;
  }
  if (this->key_in_use_(psk)) {
    ESP_LOGW(this->tag_, "Rejecting the api or ota encryption key; the peer device holds this key");
    return false;
  }
  if (this->ctx_.has_psk() && psk == this->key_) {
    ESP_LOGD(this->tag_, "Encryption key unchanged");
    return false;
  }
  if (!this->pref_.save(&psk)) {
    ESP_LOGW(this->tag_, "Failed to save encryption key, the preference storage may be full");
    return false;
  }
  // A later sync may still write the pending record, so the key is used either way
  if (!global_preferences->sync()) {
    ESP_LOGW(this->tag_, "Encryption key not yet written to flash");
  }
  this->key_ = psk;
  this->ctx_.set_psk(this->key_.data());
  // Failed handshakes under the old key do not delay the new one
  this->reset();
  this->hold_ms_ = 0;
  this->backoff_ms_ = 0;
  ESP_LOGI(this->tag_, "Encryption key saved");
  return true;
}

bool NoiseStream::key_in_use_(const psk_t &psk) const {
  psk_t other;
  if (this->api_ctx_ != nullptr) {
    // All zeros while the api has no key, which psk never is
    this->api_ctx_->load_psk(other);
    if (other == psk) {
      return true;
    }
  }
  if (this->ota_key_ != nullptr) {
    progmem_memcpy(other.data(), this->ota_key_, other.size());
    return other == psk;
  }
  return false;
}
#endif  // USE_NOISE_STREAM_RUNTIME_KEY

}  // namespace esphome::noise

#endif  // USE_NOISE_STREAM
