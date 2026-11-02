#pragma once

#include <string>

#include "esphome/components/uptime/text_sensor/uptime_text_sensor.h"

namespace esphome::uptime::testing {

static constexpr uint32_t MINUTE = 60;
static constexpr uint32_t HOUR = 60 * MINUTE;
static constexpr uint32_t DAY = 24 * HOUR;

inline UptimeTextSensor make_default(bool expand = false) { return UptimeTextSensor("d", "h", "m", "s", "", expand); }

inline UptimeTextSensor make_custom(bool expand = false) {
  return UptimeTextSensor("Days", "H", "M", "S", "-", expand);
}

inline std::string publish(UptimeTextSensor &sensor, uint32_t interval_s, uint32_t uptime_s) {
  sensor.set_update_interval(interval_s * 1000);
  sensor.publish_uptime(uptime_s);
  return sensor.get_state();
}

}  // namespace esphome::uptime::testing
