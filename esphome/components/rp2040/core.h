#pragma once

#ifdef USE_RP2040

#include <Arduino.h>
#include <pico.h>

extern "C" unsigned long ulMainGetRunTimeCounterValue();

namespace esphome {
namespace rp2040 {}  // namespace rp2040
}  // namespace esphome

#endif  // USE_RP2040
