#pragma once

// Helper macro to define a version code, whose value can be compared against other version codes.
#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))

// Branch prediction hints for performance-critical paths
#if defined(__GNUC__) || defined(__clang__)
#define ESPHOME_LIKELY(x) __builtin_expect(!!(x), 1)
#define ESPHOME_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define ESPHOME_LIKELY(x) (x)
#define ESPHOME_UNLIKELY(x) (x)
#endif

#ifdef USE_ARDUINO
#include <Arduino.h>
#endif
