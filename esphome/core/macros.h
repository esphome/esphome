#pragma once

// Helper macro to define a version code, whose value can be compared against other version codes.
#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))

// Branch prediction hints for performance-critical paths
#if defined(__GNUC__) || defined(__clang__)
// GCC and Clang: use __builtin_expect for better optimization
#define ESPHOME_LIKELY(x) __builtin_expect(!!(x), 1)
#define ESPHOME_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
// Other C++20 compilers: use standard attributes
#define ESPHOME_LIKELY(x) (x) [[likely]]
#define ESPHOME_UNLIKELY(x) (x) [[unlikely]]
#endif

#ifdef USE_ARDUINO
#include <Arduino.h>
#endif
