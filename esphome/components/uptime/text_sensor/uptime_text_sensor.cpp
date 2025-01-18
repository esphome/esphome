#include "uptime_text_sensor.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uptime {

static const char *const TAG = "uptime.sensor";

void UptimeTextSensor::setup() { this->last_ms_ = millis(); }

void UptimeTextSensor::update() {
  const auto now = millis();
  // get whole seconds since last update. Note that even if the millis count has overflowed between updates,
  // the difference will still be correct due to the way twos-complement arithmetic works.
  const uint32_t delta = (now - this->last_ms_) / 1000;
  if (delta == 0)
    return;
  // set last_ms_ to the last second boundary
  this->last_ms_ = now - (now % 1000);
  this->uptime_ += delta;
  auto uptime = this->uptime_;
  unsigned days = uptime / (24 * 3600);
  unsigned seconds = uptime % (24 * 3600);
  unsigned hours = seconds / 3600;
  seconds %= 3600;
  unsigned minutes = seconds / 60;
  seconds %= 60;
  if (days != 0) {
    this->publish_state(str_sprintf("%dd%dh%dm%ds", days, hours, minutes, seconds));
  } else if (hours != 0) {
    this->publish_state(str_sprintf("%dh%dm%ds", hours, minutes, seconds));
  } else if (minutes != 0) {
    this->publish_state(str_sprintf("%dm%ds", minutes, seconds));
  } else {
    this->publish_state(str_sprintf("%ds", seconds));
  }
}

float UptimeTextSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void UptimeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Uptime Text Sensor", this); }

}  // namespace uptime
}  // namespace esphome
