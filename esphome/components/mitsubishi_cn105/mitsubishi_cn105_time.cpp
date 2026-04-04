#include "esphome/core/application.h"

namespace esphome::mitsubishi_cn105 {

uint32_t __attribute__((weak)) get_loop_time_ms() { return App.get_loop_component_start_time(); };

}  // namespace esphome::mitsubishi_cn105
