#pragma once
#include "esphome/core/defines.h"

#ifndef USE_NATIVE_64BIT_TIME

#include <cstdint>
#include <limits>

#include "esphome/core/helpers.h"

namespace esphome {

class Scheduler;

/// Extends 32-bit millis() to 64-bit using rollover tracking.
/// Access restricted to platform HAL (millis_64()) and Scheduler.
/// All other code should call millis_64() from hal.h instead.
class Millis64Impl {
  friend uint64_t millis_64();
  friend class Scheduler;

#ifdef ESPHOME_THREAD_SINGLE
  // Storage defined in time_64.cpp — declared here so the inline body can access them.
  static uint32_t last_millis_;
  static uint16_t millis_major_;

  static inline uint64_t ESPHOME_ALWAYS_INLINE compute(uint32_t now) {
    // Half the 32-bit range - used to detect rollovers vs normal time progression
    static constexpr uint32_t HALF_MAX_UINT32 = std::numeric_limits<uint32_t>::max() / 2;

    // Single-core platforms have no concurrency, so this is a simple implementation
    // that just tracks 32-bit rollover (every 49.7 days) without any locking or atomics.
    uint16_t major = millis_major_;
    uint32_t last = last_millis_;

    // Check for rollover
    if (now < last && (last - now) > HALF_MAX_UINT32) {
      millis_major_++;
      major++;
      last_millis_ = now;
    } else if (now > last) {
      // Only update if time moved forward
      last_millis_ = now;
    }

    // Combine major (high 32 bits) and now (low 32 bits) into 64-bit time
    return now + (static_cast<uint64_t>(major) << 32);
  }
#else
  static uint64_t compute(uint32_t now);
#endif
};

}  // namespace esphome

#endif  // !USE_NATIVE_64BIT_TIME
