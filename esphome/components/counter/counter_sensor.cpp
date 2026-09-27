#include "counter_sensor.h"
#include "esphome/core/log.h"

namespace esphome::counter {

static const char *const TAG = "counter";

void CounterSensor::setup() {
  if (this->restore_) {
    this->pref_ = this->make_entity_preference<int64_t>();
    this->pref_.load(&this->value_);
  }
  this->publish_state(static_cast<float>(this->value_));
}

void CounterSensor::set_value(int64_t value) {
  this->value_ = value;
  this->publish_state(static_cast<float>(value));
  if (this->restore_)
    this->pref_.save(&this->value_);
}

void CounterSensor::dump_config() {
  LOG_SENSOR("", "Counter", this);
  ESP_LOGCONFIG(TAG, "  Restore: %s", YESNO(this->restore_));
}

}  // namespace esphome::counter
