#pragma once

#ifdef USE_RP2040

#include <Arduino.h>
#include <pico.h>

extern "C" unsigned long ulMainGetRunTimeCounterValue();

namespace esphome::rp2040 {}  // namespace esphome::rp2040

#endif  // USE_RP2040
