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
  /// psk points at 32 bytes that outlive the context (PROGMEM or caller owned
  /// RAM); nullptr means no key. Runtime callers map the all-zeros key to
  /// nullptr themselves; validation keeps it out of yaml.
  void set_psk(const uint8_t *psk) { this->psk_ = psk; }
  /// Copy the key out (flash-aware on ESP8266); all zeros when none is set.
  void load_psk(psk_t &out) const;
  bool has_psk() const { return this->psk_ != nullptr; }

 protected:
  const uint8_t *psk_{nullptr};
};

/// Convert a noise error code to a readable error
const LogString *noise_err_to_logstr(int err);

// Spare key pair layout: private key then public key
static constexpr size_t EPHEMERAL_PRIVATE_KEY_SIZE = 32;
static constexpr size_t EPHEMERAL_PUBLIC_KEY_SIZE = 32;
static constexpr size_t EPHEMERAL_KEYPAIR_SIZE = EPHEMERAL_PRIVATE_KEY_SIZE + EPHEMERAL_PUBLIC_KEY_SIZE;
using ephemeral_keypair_t = std::array<uint8_t, EPHEMERAL_KEYPAIR_SIZE>;

#ifdef USE_API_NOISE
// One responder ephemeral key pair generated ahead of time (about 60 ms on
// ESP8266), refilled by the api server while idle, shared by every noise
// transport; an empty slot means the handshake generates its own key.
extern bool spare_ephemeral_ready;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// Polled every api loop tick, so it must inline
inline bool has_spare_ephemeral() { return spare_ephemeral_ready; }
/// Fill the slot; blocks for the base point multiply
void prepare_spare_ephemeral();
/// Move the slot into out and empty it; false (out untouched) when empty
bool take_spare_ephemeral(ephemeral_keypair_t &out);
#endif

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
