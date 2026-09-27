#pragma once

#ifdef USE_RP2

#include <Arduino.h>
#include <pico.h>

// NOLINTNEXTLINE(google-runtime-int,readability-identifier-naming,readability-redundant-declaration)
extern "C" unsigned long ulMainGetRunTimeCounterValue();

namespace esphome::rp2 {}  // namespace esphome::rp2

#endif  // USE_RP2
