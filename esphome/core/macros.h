#pragma once

// Helper macro to define a version code, whose value can be compared against other version codes.
#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))

#ifdef USE_ARDUINO
#include <Arduino.h>
#endif

// Portable PROGMEM string macro
// On ESP8266, PSTR() stores strings in flash memory
// On other platforms, it's a no-op
#ifndef ESPHOME_PSTR
#ifdef USE_ESP8266
#define ESPHOME_PSTR(x) PSTR(x)
#else
#define ESPHOME_PSTR(x) (x)
#endif
#endif
