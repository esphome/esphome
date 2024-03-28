#pragma once

#include "as7343_registers.h"

namespace esphome {
namespace as7343 {

extern const float AS7343_GAIN_CORRECTION[13][12 + 1];
extern const float AS7343_CHAN_INFLUENCE_FROM_300NM[801][12];
}  // namespace as7343
}  // namespace esphome
