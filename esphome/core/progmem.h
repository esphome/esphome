#pragma once

// Platform-agnostic macros for PROGMEM string handling
// On ESP8266/Arduino: Use Arduino's F() macro for PROGMEM strings
// On other platforms: Use plain strings (no PROGMEM)

#ifdef USE_ESP8266
// ESP8266 uses Arduino macros
#define ESPHOME_F(string_literal) F(string_literal)
#define ESPHOME_PGM_P PGM_P
#define ESPHOME_strncpy_P strncpy_P
#else
#define ESPHOME_F(string_literal) (string_literal)
#define ESPHOME_PGM_P const char *
#define ESPHOME_strncpy_P strncpy
#endif
