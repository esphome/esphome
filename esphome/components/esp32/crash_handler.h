#pragma once

#ifdef USE_ESP32_CRASH_HANDLER

namespace esphome::esp32 {

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Clear the magic marker and mark crash data as consumed.
/// Call after the data has been delivered to an API client.
void crash_handler_clear();

/// Returns true if crash data was found this boot, reading it first if needed.
bool crash_handler_has_data();

}  // namespace esphome::esp32

#endif  // USE_ESP32_CRASH_HANDLER
