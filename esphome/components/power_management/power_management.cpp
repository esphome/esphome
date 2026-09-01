#include "esphome/core/log.h"
#include "power_management.h"

namespace esphome::power_management {

#ifndef USE_ESP32
void PowerManagement::setup() {}
void PowerManagement::dump_config() {}
#endif

}  // namespace esphome::power_management
