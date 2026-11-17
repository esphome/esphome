#pragma once

#include <string>

#include "esphome/components/uptime/text_sensor/uptime_text_sensor.h"

namespace esphome::uptime::testing {

static constexpr uint32_t MINUTE = 60;
static constexpr uint32_t HOUR = 60 * MINUTE;
static constexpr uint32_t DAY = 24 * HOUR;

static constexpr UptimeFormat DEFAULT_FORMAT{"d", "h", "m", "s", "", false};
static constexpr UptimeFormat DEFAULT_EXPAND_FORMAT{"d", "h", "m", "s", "", true};
static constexpr UptimeFormat CUSTOM_FORMAT{"Days", "H", "M", "S", "-", false};
static constexpr UptimeFormat CUSTOM_EXPAND_FORMAT{"Days", "H", "M", "S", "-", true};

inline UptimeTextSensor make_default(bool expand = false) {
  return UptimeTextSensor(expand ? &DEFAULT_EXPAND_FORMAT : &DEFAULT_FORMAT);
}

inline UptimeTextSensor make_custom(bool expand = false) {
  return UptimeTextSensor(expand ? &CUSTOM_EXPAND_FORMAT : &CUSTOM_FORMAT);
}

inline std::string publish(UptimeTextSensor &sensor, uint32_t interval_s, uint32_t uptime_s) {
  sensor.set_update_interval(interval_s * 1000);
  sensor.publish_uptime(uptime_s);
  return sensor.get_state();
}

}  // namespace esphome::uptime::testing
