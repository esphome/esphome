#pragma once

#ifdef USE_RP2

#include "esphome/core/defines.h"

#ifdef USE_RP2_CRASH_HANDLER

namespace esphome::rp2 {

/// Read crash data from watchdog scratch registers and clear them once per boot.
void crash_handler_read_and_clear();

/// Log crash data if a crash was detected on the previous boot.
/// Watchdog scratch registers are read and cleared on the first call per boot.
void crash_handler_log();

/// Return whether crash data was found this boot.
/// Watchdog scratch registers are read and cleared on the first call per boot.
bool crash_handler_has_data();

}  // namespace esphome::rp2

#endif  // USE_RP2_CRASH_HANDLER
#endif  // USE_RP2
