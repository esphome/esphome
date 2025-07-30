#pragma once

#include "esphome/core/defines.h"

// This file provides includes needed by the generated protobuf code
// when using pointer optimizations for component-specific types

#ifdef USE_CLIMATE
#include "esphome/components/climate/climate_mode.h"
#include "esphome/components/climate/climate_traits.h"
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

namespace esphome::api {

// This file only provides includes, no actual code

}  // namespace esphome::api
