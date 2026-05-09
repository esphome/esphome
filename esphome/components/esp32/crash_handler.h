#pragma once

#ifdef USE_ESP32_CRASH_HANDLER

namespace esphome::esp32 {

/// Read and validate crash data from NOINIT memory.
/// Does not clear the magic marker — call crash_handler_clear() after
/// the data has been delivered to an API client so it survives OTA rollback reboots.
void crash_handler_read_and_clear();

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Clear the magic marker and mark crash data as consumed.
/// Call after the data has been delivered to an API client.
void crash_handler_clear();

/// Returns true if crash data was found this boot.
bool crash_handler_has_data();

}  // namespace esphome::esp32

#endif  // USE_ESP32_CRASH_HANDLER
