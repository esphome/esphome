#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NOISE
#include <array>
#include <cstddef>
#include <cstdint>
#include "esphome/core/log.h"

namespace esphome::noise {

using psk_t = std::array<uint8_t, 32>;

class NoiseContext {
 public:
  // The all-zeros PSK is reserved: it marks the device as unprovisioned and
  // doubles as the well-known provisioning PSK that unprovisioned devices
  // accept for Noise handshakes (passive-sniffing protection only, no
  // authentication). It is never a valid real key.
  static bool is_all_zeros(const psk_t &psk) {
    uint8_t acc = 0;
    for (uint8_t b : psk) {
      acc |= b;
    }
    return acc == 0;
  }
  void set_psk(psk_t psk) {
    this->psk_ = psk;
    this->has_psk_ = !is_all_zeros(psk);
  }
  const psk_t &get_psk() const { return this->psk_; }
  bool has_psk() const { return this->has_psk_; }

 protected:
  psk_t psk_{};
  bool has_psk_{false};
};

/// Convert a noise error code to a readable error
const LogString *noise_err_to_logstr(int err);

// Shared wire format for the noise transports (api and ota): every frame is
// FRAME_INDICATOR, a 16-bit big-endian payload length, then the payload.
// Handshake payloads start with a status byte; transport payloads end with
// the ChaCha20-Poly1305 MAC.
static constexpr uint8_t FRAME_INDICATOR = 0x01;
static constexpr size_t FRAME_HEADER_SIZE = 3;
static constexpr size_t MAC_SIZE = 16;
static constexpr size_t MAX_HANDSHAKE_SIZE = 128;
static constexpr uint8_t HANDSHAKE_STATUS_OK = 0x00;
static constexpr uint8_t HANDSHAKE_STATUS_REJECT = 0x01;

inline void write_frame_header(uint8_t *buf, uint16_t payload_len) {
  buf[0] = FRAME_INDICATOR;
  buf[1] = (uint8_t) (payload_len >> 8);
  buf[2] = (uint8_t) payload_len;
}

/// Fill buf with a handshake reject payload (status byte plus the reason
/// text, PROGMEM aware); returns the payload length. buf needs capacity for
/// the status byte plus the truncated reason.
size_t format_reject_payload(uint8_t *buf, size_t capacity, const LogString *reason);

/// Reject reason for a failed handshake read. The MAC failure string is a
/// wire contract: clients match it to report a wrong key.
const LogString *reject_reason_for(int err);

/// Payload size of the MAC failure reject, the one reason string that is a
/// wire contract (sizeof's NUL stands in for the status byte). static_assert
/// reject buffers against this so a wrong key report can never truncate;
/// longer caller-supplied reasons are informational and sized by the caller.
static constexpr size_t MAC_FAILURE_PAYLOAD_SIZE = sizeof("Handshake MAC failure");

}  // namespace esphome::noise
#endif  // USE_NOISE
