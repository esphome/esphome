#pragma once

#ifdef USE_ESP8266

#include "esphome/core/defines.h"

#ifdef USE_ESP8266_CRASH_HANDLER

namespace esphome::esp8266 {

/// Read crash data from rst_info and RTC user memory, then clear RTC data.
void crash_handler_read_and_clear();

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Returns true if crash data was found this boot.
bool crash_handler_has_data();

}  // namespace esphome::esp8266

#endif  // USE_ESP8266_CRASH_HANDLER
#endif  // USE_ESP8266
