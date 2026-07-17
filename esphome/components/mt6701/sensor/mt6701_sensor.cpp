#include "mt6701_sensor.h"
#include "esphome/core/log.h"

namespace esphome::mt6701 {

static const char *const TAG = "mt6701.sensor";

void MT6701Sensor::update() {
  // A hub that failed setup must not be polled: on SSI, bus noise can pass the
  // 6-bit CRC and would publish garbage from a dead device.
  if (this->parent_->is_failed())
    return;
  // Trigger a fresh read; skip publishing when it failed so we never emit a
  // stale or garbage value.
  if (!this->parent_->read_encoder())
    return;

  this->publish_state(this->parent_->get_angle_degrees());
  if (this->raw_count_sensor_ != nullptr)
    this->raw_count_sensor_->publish_state(this->parent_->get_count());
}

void MT6701Sensor::dump_config() {
  LOG_SENSOR("", "MT6701 Sensor", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Raw Count", this->raw_count_sensor_);
}

}  // namespace esphome::mt6701
