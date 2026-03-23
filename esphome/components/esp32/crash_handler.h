#pragma once

#ifdef USE_ESP32_CRASH_HANDLER

namespace esphome::esp32 {

/// Read crash data from NOINIT memory and clear the magic marker.
void crash_handler_read_and_clear();

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Returns true if crash data was found this boot.
bool crash_handler_has_data();

}  // namespace esphome::esp32

#endif  // USE_ESP32_CRASH_HANDLER
