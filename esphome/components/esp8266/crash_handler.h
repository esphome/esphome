#pragma once

#ifdef USE_ESP8266

#include "esphome/core/defines.h"

#ifdef USE_ESP8266_CRASH_HANDLER

namespace esphome::esp8266 {

/// Log crash data if a crash was detected on previous boot.
void crash_handler_log();

/// Returns true if the previous boot was a crash (exception, WDT, or soft WDT).
bool crash_handler_has_data();

}  // namespace esphome::esp8266

#endif  // USE_ESP8266_CRASH_HANDLER
#endif  // USE_ESP8266
