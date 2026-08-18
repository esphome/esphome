#pragma once
#include "esphome/core/defines.h"

#ifdef USE_API_NOISE
#include "esphome/components/noise/noise.h"

namespace esphome::api {

// Kept as aliases for external components that use the api names
using psk_t = noise::psk_t;
using APINoiseContext = noise::NoiseContext;

}  // namespace esphome::api
#endif  // USE_API_NOISE
