#pragma once
#include <array>
#include <cstdint>
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

#ifdef USE_API_NOISE
using psk_t = std::array<uint8_t, 32>;

class APINoiseContext {
 public:
  void set_psk(psk_t psk) {
    this->psk_ = psk;
    bool has_psk = false;
    for (auto i : psk) {
      has_psk |= i;
    }
    this->has_psk_ = has_psk;
  }
  const psk_t &get_psk() const { return this->psk_; }
  bool has_psk() const { return this->has_psk_; }

 protected:
  psk_t psk_{};
  bool has_psk_{false};
};
#endif  // USE_API_NOISE

}  // namespace api
}  // namespace esphome
