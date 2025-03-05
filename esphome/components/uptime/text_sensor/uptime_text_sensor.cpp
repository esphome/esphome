#include "uptime_text_sensor.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uptime {

static const char *const TAG = "uptime.sensor";

void UptimeTextSensor::setup() {
  this->last_ms_ = millis();
  if (this->last_ms_ < 60 * 1000)
    this->last_ms_ = 0;
  this->update();
}

void UptimeTextSensor::update() {
  auto now = millis();
  // get whole seconds since last update. Note that even if the millis count has overflowed between updates,
  // the difference will still be correct due to the way twos-complement arithmetic works.
  uint32_t delta = now - this->last_ms_;
  this->last_ms_ = now - delta % 1000;  // save remainder for next update
  delta /= 1000;
  this->uptime_ += delta;
  auto uptime = this->uptime_;
  unsigned interval = this->get_update_interval() / 1000;
  std::string buffer{};
  // display from the largest unit that corresponds to the update interval, drop larger units that are zero.
  while (true) {  // enable use of break for early exit
    unsigned remainder = uptime % 60;
    uptime /= 60;
    if (interval < 30) {
      buffer.insert(0, str_sprintf("%us", remainder));
      if (uptime == 0)
        break;
    }
    remainder = uptime % 60;
    uptime /= 60;
    if (interval < 1800) {
      buffer.insert(0, str_sprintf("%um", remainder));
      if (uptime == 0)
        break;
    }
    remainder = uptime % 24;
    uptime /= 24;
    if (interval < 12 * 3600) {
      buffer.insert(0, str_sprintf("%uh", remainder));
      if (uptime == 0)
        break;
    }
    buffer.insert(0, str_sprintf("%ud", (unsigned) uptime));
    break;
  }
  this->publish_state(buffer);
}

float UptimeTextSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void UptimeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Uptime Text Sensor", this); }

}  // namespace uptime
}  // namespace esphome
