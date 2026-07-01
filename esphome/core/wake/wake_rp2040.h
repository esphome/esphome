#pragma once

#include "esphome/core/defines.h"

#ifdef USE_RP2040

#include "esphome/core/hal.h"

#include <hardware/sync.h>
#include <pico/time.h>

namespace esphome {

inline void wake_loop_any_context() {
  // Set the wake-requested flag BEFORE the SEV so the consumer is guaranteed
  // to see it on its next gate check.
  wake_request_set();
  g_main_loop_woke = true;
  __sev();
}

inline void wake_loop_threadsafe() { wake_loop_any_context(); }

/// RP2040 wakeable delay uses file-scope state (alarm callback + flag) — defined in wake_rp2040.cpp.
namespace internal {
void wakeable_delay(uint32_t ms);
}  // namespace internal

}  // namespace esphome

#endif  // USE_RP2040
