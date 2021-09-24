#pragma once
#include <cstdint>
#include <array>
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

#ifdef USE_API_NOISE
using psk_t = std::array<uint8_t, 32>;

class APINoiseContext {
 public:
  void set_psk(psk_t psk) { psk_ = psk; }
  const psk_t &get_psk() const { return psk_; }

 protected:
  psk_t psk_;
};
#endif  // USE_API_NOISE

}  // namespace api
}  // namespace esphome
