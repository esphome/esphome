#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR

#include "esphome/core/hal.h"

namespace esphome {

/// Zephyr: wakes the main loop via k_sem_give(). Thread- and ISR-safe.
/// Defined in wake_zephyr.cpp.
void wake_loop_threadsafe();

inline void wake_loop_any_context() { wake_loop_threadsafe(); }

/// ISR-safe: no task_woken arg because Zephyr's k_sem_give() does its own ISR
/// scheduling. Forwards to wake_loop_threadsafe().
inline void wake_loop_isrsafe() { wake_loop_threadsafe(); }

namespace internal {
/// Zephyr wakeable_delay uses k_sem_take() with a timeout — defined in wake_zephyr.cpp.
void wakeable_delay(uint32_t ms);
}  // namespace internal

}  // namespace esphome

#endif  // USE_ZEPHYR
