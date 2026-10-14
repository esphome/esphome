#pragma once

#ifdef USE_ESP8266

#include <cstdint>

namespace esphome::esp8266 {

// User RTC memory is system blocks 64-191, one 4-byte word each. preferences.cpp owns the first
// words, crash_handler.cpp the last; const.py hands preferences.cpp its region sizes.
static constexpr uint32_t RTC_SYSTEM_USER_BASE = 64;
static constexpr uint32_t RTC_USER_MEM_WORDS = 128;
static constexpr uint32_t RTC_CRASH_WORDS = 18;
static constexpr uint32_t RTC_CRASH_BASE = RTC_SYSTEM_USER_BASE + RTC_USER_MEM_WORDS - RTC_CRASH_WORDS;

}  // namespace esphome::esp8266

#endif  // USE_ESP8266
