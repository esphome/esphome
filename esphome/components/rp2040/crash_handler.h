#pragma once

#ifdef USE_RP2040

#include "esphome/core/defines.h"

#ifdef USE_RP2040_CRASH_HANDLER

namespace esphome::rp2040 {

/// Read crash data from watchdog scratch registers and clear them.
void crash_handler_read_and_clear();

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Returns true if crash data was found this boot.
bool crash_handler_has_data();

}  // namespace esphome::rp2040

#endif  // USE_RP2040_CRASH_HANDLER
#endif  // USE_RP2040
