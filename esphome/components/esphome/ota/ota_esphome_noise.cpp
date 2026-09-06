#include "ota_esphome.h"
#ifdef USE_OTA
#ifdef USE_OTA_ENCRYPTION
#include "esphome/components/noise/noise.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/log.h"

#include <cstring>
#include <new>

#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome {

static const char *const TAG = "esphome.ota";

#ifdef USE_ESP8266
static constexpr char OTA_NOISE_PROLOGUE_INIT[] PROGMEM = "NoiseOTAInit";
#else
static constexpr char OTA_NOISE_PROLOGUE_INIT[] = "NoiseOTAInit";
#endif
static constexpr size_t OTA_NOISE_PROLOGUE_INIT_LEN = sizeof(OTA_NOISE_PROLOGUE_INIT) - 1;

ESPHomeOTAComponent::NoiseSession::~NoiseSession() {
  if (this->send_cipher != nullptr) {
    noise_cipherstate_free(this->send_cipher);
  }
  if (this->recv_cipher != nullptr) {
    noise_cipherstate_free(this->recv_cipher);
  }
}

/** Allocate the session and start the responder handshake.
 *
 * The prologue binds the whole plaintext preamble, so any tampering with the
 * negotiation (a stripped feature flag, a changed version) breaks the first
 * handshake MAC on either side:
 *   "NoiseOTAInit" | magic(5) | OK,version | client_features | FEATURE_FLAGS,server_flags
 */
bool ESPHomeOTAComponent::noise_start_session_(uint8_t server_feature_flags) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  this->noise_ = std::unique_ptr<NoiseSession>(new (std::nothrow) NoiseSession());
  if (this->noise_ == nullptr) {
    ESP_LOGW(TAG, "Session allocation failed");
    this->cleanup_connection_();
    return false;
  }

  static constexpr size_t PROLOGUE_ACK_LEN = 2;  // OTA_RESPONSE_OK + version
  static constexpr size_t PROLOGUE_CLIENT_FEATURES_LEN = 1;
  static constexpr size_t PROLOGUE_FEATURE_ACK_LEN = 2;  // OTA_RESPONSE_FEATURE_FLAGS + server flags
  uint8_t prologue[OTA_NOISE_PROLOGUE_INIT_LEN + sizeof(MAGIC_BYTES) + PROLOGUE_ACK_LEN + PROLOGUE_CLIENT_FEATURES_LEN +
                   PROLOGUE_FEATURE_ACK_LEN];
#ifdef USE_ESP8266
  memcpy_P(prologue, OTA_NOISE_PROLOGUE_INIT, OTA_NOISE_PROLOGUE_INIT_LEN);
#else
  std::memcpy(prologue, OTA_NOISE_PROLOGUE_INIT, OTA_NOISE_PROLOGUE_INIT_LEN);
#endif
  uint8_t *p = prologue + OTA_NOISE_PROLOGUE_INIT_LEN;
  // Magic bytes, already validated in MAGIC_READ
  std::memcpy(p, MAGIC_BYTES, sizeof(MAGIC_BYTES));
  p += sizeof(MAGIC_BYTES);
  // Our magic ack
  *p++ = ota::OTA_RESPONSE_OK;
  *p++ = USE_OTA_VERSION;
  // The feature byte the client sent
  *p++ = this->ota_features_;
  // The feature ack we sent (noise requires the extended protocol)
  *p++ = ota::OTA_RESPONSE_FEATURE_FLAGS;
  *p++ = server_feature_flags;

  int err = this->noise_->handshake.init(this->noise_ctx_.get_psk(), prologue, sizeof(prologue));
  if (err != 0) {
    ESP_LOGW(TAG, "Handshake init: %s", LOG_STR_ARG(noise::noise_err_to_logstr(err)));
    this->cleanup_connection_();
    return false;
  }
  return true;
}

/** Drive the non-blocking handshake from loop(); returns true once the
 * transport ciphers are ready. A would-block returns false and the next
 * loop() resumes from the NoiseSession cursors; on failure the connection
 * is cleaned up.
 */
bool ESPHomeOTAComponent::handle_noise_handshake_() {
  NoiseSession &s = *this->noise_;
  while (true) {
    if (s.writing) {
      if (!this->noise_try_write_frame_()) {
        return false;  // would block, or errored and cleaned up
      }
      s.writing = false;
      s.frame_pos = 0;
      s.frame_len = 0;
    }
    switch (s.handshake.action()) {
      case noise::NoiseResponderHandshake::Action::ACTION_READ: {
        if (!this->noise_try_read_frame_()) {
          return false;
        }
        const uint16_t payload_len = s.frame_len - noise::FRAME_HEADER_SIZE;
        s.frame_pos = 0;
        s.frame_len = 0;
        if (s.frame_buf[noise::FRAME_HEADER_SIZE] != noise::HANDSHAKE_STATUS_OK) {
          ESP_LOGW(TAG, "Bad handshake error byte: %u", s.frame_buf[noise::FRAME_HEADER_SIZE]);
          this->cleanup_connection_();
          return false;
        }
        int err = s.handshake.read_message(s.frame_buf + noise::FRAME_HEADER_SIZE + 1, payload_len - 1);
        if (err != 0) {
          ESP_LOGW(TAG, "Handshake read: %s", LOG_STR_ARG(noise::noise_err_to_logstr(err)));
          this->noise_send_reject_(noise::reject_reason_for(err));
          this->cleanup_connection_();
          return false;
        }
        break;
      }
      case noise::NoiseResponderHandshake::Action::ACTION_WRITE: {
        size_t msg_len = 0;
        int err =
            s.handshake.write_message(s.frame_buf + noise::FRAME_HEADER_SIZE + 1, noise::MAX_HANDSHAKE_SIZE, msg_len);
        if (err != 0) {
          ESP_LOGW(TAG, "Handshake write: %s", LOG_STR_ARG(noise::noise_err_to_logstr(err)));
          this->cleanup_connection_();
          return false;
        }
        const uint16_t payload_len = msg_len + 1;
        noise::write_frame_header(s.frame_buf, payload_len);
        s.frame_buf[noise::FRAME_HEADER_SIZE] = noise::HANDSHAKE_STATUS_OK;
        s.frame_len = noise::FRAME_HEADER_SIZE + payload_len;
        s.frame_pos = 0;
        s.writing = true;
        break;
      }
      case noise::NoiseResponderHandshake::Action::ACTION_SPLIT: {
        int err = s.handshake.split(s.send_cipher, s.recv_cipher);
        if (err != 0) {
          ESP_LOGW(TAG, "Handshake split: %s", LOG_STR_ARG(noise::noise_err_to_logstr(err)));
          this->cleanup_connection_();
          return false;
        }
        ESP_LOGD(TAG, "Noise handshake complete");
        return true;
      }
      default: {
        ESP_LOGW(TAG, "Bad handshake state");
        this->cleanup_connection_();
        return false;
      }
    }
  }
}

/// Non-blocking read of one handshake frame into the session buffer.
bool ESPHomeOTAComponent::noise_try_read_frame_() {
  NoiseSession &s = *this->noise_;
  while (s.frame_pos < noise::FRAME_HEADER_SIZE) {
    ssize_t read = this->client_->read(s.frame_buf + s.frame_pos, noise::FRAME_HEADER_SIZE - s.frame_pos);
    if (!this->handle_read_error_(read, LOG_STR("read noise header"))) {
      return false;
    }
    s.frame_pos += read;
  }
  if (s.frame_len == 0) {
    const uint16_t payload_len = encode_uint16(s.frame_buf[1], s.frame_buf[2]);
    if (s.frame_buf[0] != noise::FRAME_INDICATOR || payload_len < 1 || payload_len > 1 + noise::MAX_HANDSHAKE_SIZE) {
      ESP_LOGW(TAG, "Bad handshake frame: 0x%02X, %u bytes", s.frame_buf[0], payload_len);
      this->cleanup_connection_();
      return false;
    }
    s.frame_len = noise::FRAME_HEADER_SIZE + payload_len;
  }
  while (s.frame_pos < s.frame_len) {
    ssize_t read = this->client_->read(s.frame_buf + s.frame_pos, s.frame_len - s.frame_pos);
    if (!this->handle_read_error_(read, LOG_STR("read noise frame"))) {
      return false;
    }
    s.frame_pos += read;
  }
  return true;
}

/// Non-blocking write of the pending session-buffer frame.
bool ESPHomeOTAComponent::noise_try_write_frame_() {
  NoiseSession &s = *this->noise_;
  while (s.frame_pos < s.frame_len) {
    ssize_t written = this->client_->write(s.frame_buf + s.frame_pos, s.frame_len - s.frame_pos);
    if (!this->handle_write_error_(written, LOG_STR("write noise frame"))) {
      return false;
    }
    s.frame_pos += written;
  }
  return true;
}

/// Best-effort explicit reject frame so the client can log a readable reason.
void ESPHomeOTAComponent::noise_send_reject_(const LogString *reason) {
  // Every reason here comes from noise::reject_reason_for(), so the exported
  // floor is the exact capacity needed
  uint8_t data[noise::FRAME_HEADER_SIZE + noise::MAC_FAILURE_PAYLOAD_SIZE];
  const size_t payload_len =
      noise::format_reject_payload(data + noise::FRAME_HEADER_SIZE, sizeof(data) - noise::FRAME_HEADER_SIZE, reason);
  noise::write_frame_header(data, payload_len);
  this->client_->write(data, noise::FRAME_HEADER_SIZE + payload_len);  // Best effort, non-blocking
}

/// Decrypt a ciphertext in place; returns the plaintext size or -1.
ssize_t ESPHomeOTAComponent::noise_decrypt_(uint8_t *buf, size_t len) {
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_inout(mbuf, buf, len, len);
  int err = noise_cipherstate_decrypt(this->noise_->recv_cipher, &mbuf);
  if (err != 0) {
    ESP_LOGW(TAG, "Decrypt: %s", LOG_STR_ARG(noise::noise_err_to_logstr(err)));
    return -1;
  }
  return mbuf.size;
}

/** Blocking read of one frame whose ciphertext size must be within the given
 * bounds, decrypted in place; returns the plaintext size, or -1 on error.
 * buf needs max_ciphertext capacity.
 */
ssize_t ESPHomeOTAComponent::noise_read_frame_blocking_(uint8_t *buf, size_t min_ciphertext, size_t max_ciphertext) {
  uint8_t header[noise::FRAME_HEADER_SIZE];
  if (!this->readall_(header, sizeof(header))) {
    return -1;
  }
  const size_t ciphertext_len = encode_uint16(header[1], header[2]);
  if (header[0] != noise::FRAME_INDICATOR || ciphertext_len < min_ciphertext || ciphertext_len > max_ciphertext) {
    ESP_LOGW(TAG, "Bad frame: 0x%02X, %zu bytes", header[0], ciphertext_len);
    return -1;
  }
  if (!this->readall_(buf, ciphertext_len)) {
    return -1;
  }
  return this->noise_decrypt_(buf, ciphertext_len);
}

/** Blocking read of one frame whose plaintext must be exactly len bytes
 * (control units are one unit per frame). buf needs len + noise::MAC_SIZE
 * capacity; the plaintext lands at buf[0..len).
 */
bool ESPHomeOTAComponent::noise_readall_(uint8_t *buf, size_t len) {
  return this->noise_read_frame_blocking_(buf, len + noise::MAC_SIZE, len + noise::MAC_SIZE) == (ssize_t) len;
}

/** Blocking read of one data-phase frame, decrypted in place; returns the
 * plaintext size, or -1 on error. buf is the OTA_BUFFER_SIZE data buffer.
 * The ciphertext must fit that buffer and its plaintext must fit what the
 * caller accepts (the remaining image bytes).
 */
ssize_t ESPHomeOTAComponent::noise_read_data_(uint8_t *buf, size_t capacity) {
  const size_t max_ciphertext = std::min(capacity + noise::MAC_SIZE, OTA_BUFFER_SIZE);
  return this->noise_read_frame_blocking_(buf, noise::MAC_SIZE + 1, max_ciphertext);
}

/// Blocking write of one response byte as an encrypted frame.
bool ESPHomeOTAComponent::noise_write_byte_(uint8_t byte) {
  uint8_t frame[noise::FRAME_HEADER_SIZE + 1 + noise::MAC_SIZE];
  frame[noise::FRAME_HEADER_SIZE] = byte;
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_inout(mbuf, frame + noise::FRAME_HEADER_SIZE, 1, 1 + noise::MAC_SIZE);
  int err = noise_cipherstate_encrypt(this->noise_->send_cipher, &mbuf);
  if (err != 0) {
    ESP_LOGW(TAG, "Encrypt: %s", LOG_STR_ARG(noise::noise_err_to_logstr(err)));
    return false;
  }
  noise::write_frame_header(frame, mbuf.size);
  return this->writeall_(frame, noise::FRAME_HEADER_SIZE + mbuf.size);
}

}  // namespace esphome
#endif  // USE_OTA_ENCRYPTION
#endif  // USE_OTA
