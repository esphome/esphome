#pragma once

#include "esphome/core/defines.h"
#ifdef USE_STORE_YAML

#include <cstdint>

#include "esphome/core/hal.h"

namespace esphome::store_yaml {

/// Published in GetYamlResponse.encoding so clients know how to decompress.
constexpr const char *ENCODING = "zstd";

/// The YAML envelope as zstd, built by codegen and kept in flash. STORE_YAML_DATA_SIZE bytes long.
extern const uint8_t STORE_YAML_DATA[] PROGMEM;

}  // namespace esphome::store_yaml

#endif  // USE_STORE_YAML
