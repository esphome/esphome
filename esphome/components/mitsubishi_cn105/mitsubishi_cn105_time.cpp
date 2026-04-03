#include "esphome/core/application.h"

namespace esphome::mitsubishi_cn105 {

#ifndef MITSUBISHI_CN105_UNIT_TEST
uint32_t get_loop_time_ms() { return App.get_loop_component_start_time(); };
#endif

}  // namespace esphome::mitsubishi_cn105
