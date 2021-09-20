#include "duty_cycle_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace duty_cycle {

static const char *const TAG = "duty_cycle";

void DutyCycleSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Duty Cycle Sensor '%s'...", this->get_name().c_str());
  this->pin_->setup();
  this->store_.pin = this->pin_->to_isr();
  this->store_.last_level = this->pin_->digital_read();
  this->last_update_ = micros();
  this->store_.last_interrupt = micros();

  this->pin_->attach_interrupt(DutyCycleSensorStore::gpio_intr, &this->store_, CHANGE);
}
void DutyCycleSensor::dump_config() {
  LOG_SENSOR("", "Duty Cycle Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}
void DutyCycleSensor::update() {
  const uint32_t now = micros();
  const bool level = this->store_.last_level;
  const uint32_t last_interrupt = this->store_.last_interrupt;
  uint32_t on_time = this->store_.on_time;

  if (level)
    on_time += now - last_interrupt;

  const float total_time = float(now - this->last_update_);

  const float value = (on_time / total_time) * 100.0f;
  ESP_LOGD(TAG, "'%s' Got duty cycle=%.1f%%", this->get_name().c_str(), value);
  this->publish_state(value);

  this->store_.on_time = 0;
  this->store_.last_interrupt = now;
  this->last_update_ = now;
}

float DutyCycleSensor::get_setup_priority() const { return setup_priority::DATA; }

void ICACHE_RAM_ATTR DutyCycleSensorStore::gpio_intr(DutyCycleSensorStore *arg) {
  const bool new_level = arg->pin->digital_read();
  if (new_level == arg->last_level)
    return;
  arg->last_level = new_level;
  const uint32_t now = micros();

  if (!new_level)
    arg->on_time += now - arg->last_interrupt;

  arg->last_interrupt = now;
}

}  // namespace duty_cycle
}  // namespace esphome
