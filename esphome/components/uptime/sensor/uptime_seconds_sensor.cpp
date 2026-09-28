#include "uptime_seconds_sensor.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::uptime {

static const char *const TAG = "uptime.sensor";

void UptimeSecondsSensor::update() {
  const uint64_t uptime = millis_64();
  const uint64_t seconds_int = uptime / 1000ULL;
  const float seconds = float(seconds_int) + (uptime % 1000ULL) / 1000.0f;
  this->publish_state(seconds);
}
float UptimeSecondsSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void UptimeSecondsSensor::dump_config() {
  LOG_SENSOR("", "Uptime Sensor", this);
  ESP_LOGCONFIG(TAG, "  Type: Seconds");
}

}  // namespace esphome::uptime
