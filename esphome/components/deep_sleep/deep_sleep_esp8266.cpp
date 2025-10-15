#ifdef USE_ESP8266
#include "deep_sleep_component.h"

#include <Esp.h>

namespace esphome {
namespace deep_sleep {

static const char *const TAG = "deep_sleep";

optional<uint32_t> DeepSleepComponent::get_run_duration_() const { return this->run_duration_; }

void DeepSleepComponent::dump_config_platform_() {}

bool DeepSleepComponent::prepare_to_sleep_() { return true; }

void DeepSleepComponent::deep_sleep_() {
  ESP.deepSleep(*this->sleep_duration_);  // NOLINT(readability-static-accessed-through-instance)
}

}  // namespace deep_sleep
}  // namespace esphome
#endif
