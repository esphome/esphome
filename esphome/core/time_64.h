#pragma once
#include "esphome/core/defines.h"

#ifndef USE_NATIVE_64BIT_TIME

#include <cstdint>

namespace esphome {

class Scheduler;

/// Extends 32-bit millis() to 64-bit using rollover tracking.
/// Access restricted to platform HAL (millis_64()) and Scheduler.
/// All other code should call millis_64() from hal.h instead.
class Millis64Impl {
  friend uint64_t millis_64();
  friend class Scheduler;

  static uint64_t compute(uint32_t now);
};

}  // namespace esphome

#endif  // !USE_NATIVE_64BIT_TIME
