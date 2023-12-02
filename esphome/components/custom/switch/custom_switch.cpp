#include "custom_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace custom {

static const char *const TAG = "custom.switch";

void CustomSwitchConstructor::dump_config() {
  for (auto *child : this->switches_) {
    LOG_SWITCH("", "Custom Switch", child);
  }
}

}  // namespace custom
}  // namespace esphome
