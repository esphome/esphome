#ifdef USE_ESP8266
#include "deep_sleep_component.h"

#include <Esp.h>

#ifdef USE_DEEP_SLEEP_ON_WAKE
extern "C" {
#include <user_interface.h>
}
#endif

namespace esphome::deep_sleep {

static const char *const TAG = "deep_sleep";

#ifdef USE_DEEP_SLEEP_ON_WAKE
WakeupCause get_wakeup_cause() {
  // The ESP8266 can only wake from deep sleep through the RTC timer (via GPIO16 -> RST).
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  if (ESP.getResetInfoPtr()->reason == REASON_DEEP_SLEEP_AWAKE) {
    return WAKEUP_CAUSE_TIMER;
  }
  return WAKEUP_CAUSE_NONE;
}
#endif  // USE_DEEP_SLEEP_ON_WAKE

optional<uint32_t> DeepSleepComponent::get_run_duration_() const { return this->run_duration_; }

void DeepSleepComponent::dump_config_platform_() {}

bool DeepSleepComponent::prepare_to_sleep_() { return true; }

void DeepSleepComponent::deep_sleep_() {
  ESP.deepSleep(this->sleep_duration_.value_or(0));  // NOLINT(readability-static-accessed-through-instance)
}

bool DeepSleepComponent::should_teardown_() { return true; }

}  // namespace esphome::deep_sleep
#endif
