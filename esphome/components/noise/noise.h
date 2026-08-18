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

}  // namespace esphome::noise
#endif  // USE_NOISE
