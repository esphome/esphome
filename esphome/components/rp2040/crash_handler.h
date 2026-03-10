#pragma once

#ifdef USE_RP2040

#include <cstdint>

namespace esphome::rp2040 {

/// Read crash data from watchdog scratch registers and clear them.
void crash_handler_read_and_clear();

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

}  // namespace esphome::rp2040

#endif  // USE_RP2040
