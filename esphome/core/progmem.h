#pragma once

// Platform-agnostic macros for PROGMEM string handling
// On ESP32 (both Arduino and IDF): Use plain strings (no PROGMEM)
// On ESP8266/Arduino: Use Arduino's F() macro for PROGMEM strings

#ifdef USE_ESP32
#define ESPHOME_F(string_literal) (string_literal)
#define ESPHOME_PGM_P const char *
#define ESPHOME_strncpy_P strncpy
#else
// ESP8266 and other Arduino platforms use Arduino macros
#define ESPHOME_F(string_literal) F(string_literal)
#define ESPHOME_PGM_P PGM_P
#define ESPHOME_strncpy_P strncpy_P
#endif
