#pragma once

#include "esphome/core/defines.h"

// This file provides includes needed by the generated protobuf code
// when using pointer optimizations for component-specific types

#ifdef USE_CLIMATE
#include "esphome/components/climate/climate_mode.h"
#include "esphome/components/climate/climate_traits.h"
#endif

#ifdef USE_WATER_HEATER
#include "esphome/components/water_heater/water_heater.h"
#endif

#ifdef USE_LIGHT
#include "esphome/components/light/light_traits.h"
#endif

#ifdef USE_FAN
#include "esphome/components/fan/fan_traits.h"
#endif

#ifdef USE_SELECT
#include "esphome/components/select/select_traits.h"
#endif

// Standard library includes that might be needed
#include <set>
#include <vector>
#include <string>

#if defined(LOG_LEVEL_NONE)
// Zephyr defines LOG_LEVEL_NONE as a logging macro that collides with the LogLevel enum value of
// the same name in the generated api_pb2.h. Undefine it for the rest of this translation unit so
// the enum parses; nothing below needs Zephyr's logging macro.
#undef LOG_LEVEL_NONE
#endif

namespace esphome::api {

// This file only provides includes, no actual code

}  // namespace esphome::api
