#pragma once

#include <array>
#include <cstdint>
#include "esphome/core/hal.h"

namespace esphome {
namespace gpio_expander {

/// @brief A class to cache the read state of a GPIO expander.
template<typename T, T N> class CachedGpioExpander {
 public:
  bool digital_read(T pin) {
    if (!this->read_cache_invalidated_[pin]) {
      this->read_cache_invalidated_[pin] = true;
      return this->digital_read_cache(pin);
    }
    return this->digital_read_hw(pin);
  }

  void digital_write(T pin, bool value) { this->digital_write_hw(pin, value); }

 protected:
  virtual bool digital_read_hw(T pin) = 0;
  virtual bool digital_read_cache(T pin) = 0;
  virtual void digital_write_hw(T pin, bool value) = 0;

  void reset_pin_cache_() {
    for (T i = 0; i < N; i++) {
      this->read_cache_invalidated_[i] = false;
    }
  }

  std::array<bool, N> read_cache_invalidated_{};
};

}  // namespace gpio_expander
}  // namespace esphome
