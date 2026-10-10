#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DS3231_SQUARE_WAVE

#include "esphome/components/select/select.h"

#include "../ds3231.h"

namespace esphome::ds3231 {

/// Selects what the INT/SQW pin does: index 0 = alarm-interrupt line, index 1 = square-wave
/// output. The two functions share one pin and are mutually exclusive.
class DS3231OutputModeSelect final : public select::Select, public Parented<DS3231Component> {
 protected:
  void control(size_t index) override;
};

/// Selects the square-wave frequency. The four options map directly onto DS3231SquareWaveFrequency
/// (index 0 = 1 Hz, 1 = 1.024 kHz, 2 = 4.096 kHz, 3 = 8.192 kHz) and are fixed by the hardware.
class DS3231SquareWaveFrequencySelect final : public select::Select, public Parented<DS3231Component> {
 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ds3231

#endif  // USE_DS3231_SQUARE_WAVE
