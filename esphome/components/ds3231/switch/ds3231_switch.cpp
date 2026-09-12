#include "ds3231_switch.h"
#include "esphome/core/log.h"

namespace esphome::ds3231 {

static const char *const TAG = "ds3231.switch";

#ifdef USE_DS3231_32KHZ_OUTPUT
void DS3231Enable32kHzSwitch::write_state(bool state) {
  if (this->parent_->set_32khz_output(state)) {
    this->publish_state(state);
  } else {
    ESP_LOGW(TAG, "Setting 32 kHz output failed.");
  }
}
#endif

#ifdef USE_DS3231_ALARM
void DS3231AlarmSwitch::write_state(bool state) {
  if (this->parent_->set_alarm_enabled(this->alarm_, state)) {
    this->publish_state(state);
  } else {
    ESP_LOGW(TAG, "Setting alarm %u enable failed.", this->alarm_);
  }
}
#endif

}  // namespace esphome::ds3231
