#pragma once

#include "esphome/core/defines.h"
#ifdef USE_STORE_YAML

#include <cstdint>

#include "esphome/core/hal.h"

namespace esphome::store_yaml {

/// The zstd YAML envelope, STORE_YAML_DATA_SIZE bytes in flash.
extern const uint8_t STORE_YAML_DATA[] PROGMEM;

}  // namespace esphome::store_yaml

#endif  // USE_STORE_YAML
