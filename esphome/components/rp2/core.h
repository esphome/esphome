#pragma once

#ifdef USE_RP2

#include <Arduino.h>
#include <pico.h>

extern "C" unsigned long ulMainGetRunTimeCounterValue();

namespace esphome::rp2 {}  // namespace esphome::rp2

#endif  // USE_RP2
