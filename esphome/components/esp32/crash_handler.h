#pragma once

#ifdef USE_ESP32_CRASH_HANDLER

namespace esphome::esp32 {

/// Log crash data if a crash was detected on the previous boot.
/// Retained data is read and validated on the first call per boot.
void crash_handler_log();

/// Clear the retained magic marker after the data has been delivered to an API client.
/// The validated in-memory data remains available for other clients during this boot.
void crash_handler_clear();

/// Return whether crash data was found and validated this boot.
/// Retained data is read and validated on the first call per boot.
bool crash_handler_has_data();

}  // namespace esphome::esp32

#endif  // USE_ESP32_CRASH_HANDLER
