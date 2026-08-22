#pragma once
#include <array>
#include <cstdint>
#include "esphome/core/defines.h"

namespace esphome::api {

#ifdef USE_API_NOISE
using psk_t = std::array<uint8_t, 32>;

class APINoiseContext {
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
#endif  // USE_API_NOISE

}  // namespace esphome::api
