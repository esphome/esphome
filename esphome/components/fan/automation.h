#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "fan.h"

namespace esphome::fan {

/// Codegen use only: step `fan` to its next speed. Past the top speed it turns off when `off_speed_cycle` is set,
/// otherwise it goes back to speed 1. A fan without speeds is toggled.
void cycle_speed(Fan *fan, bool off_speed_cycle);

}  // namespace esphome::fan
