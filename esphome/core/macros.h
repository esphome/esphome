#pragma once

// Helper macro to define a version code, whose value can be compared against other version codes.
#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))

#ifdef USE_ARDUINO
#include <Arduino.h>
#endif

#ifdef USE_ZEPHYR
#define M_PI 3.14159265358979323846
#endif
