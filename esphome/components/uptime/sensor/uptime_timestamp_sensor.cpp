#include "uptime_timestamp_sensor.h"

#ifdef USE_TIME

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uptime {

static const char *const TAG = "uptime.sensor";

void UptimeTimestampSensor::setup() {
  this->time_->add_on_time_sync_callback([this]() {
    if (this->has_state_)
      return;  // No need to update the timestamp if it's already set

    auto now = this->time_->now();
    const uint32_t ms = millis();
    if (!now.is_valid())
      return;  // No need to update the timestamp if the time is not valid

    time_t timestamp = now.timestamp;
    uint32_t seconds = ms / 1000;
    timestamp -= seconds;
    this->publish_state(timestamp);
  });
}
float UptimeTimestampSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void UptimeTimestampSensor::dump_config() {
  LOG_SENSOR("", "Uptime Sensor", this);
  ESP_LOGCONFIG(TAG, "  Type: Timestamp");
}

}  // namespace uptime
}  // namespace esphome

#endif  // USE_TIME
