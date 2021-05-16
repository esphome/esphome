#include "interrupt_pulse_counter_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace interrupt_pulse_counter {

static const char *TAG = "interrupt_pulse_counter";

const char *EDGE_MODE_TO_STRING[] = {"DISABLE", "INCREMENT", "DECREMENT"};

void ICACHE_RAM_ATTR InterruptPulseCounterStorage::gpio_intr(InterruptPulseCounterStorage *arg) {
  const uint32_t now = micros();
  const bool discard = now - arg->last_pulse < arg->filter_us;
  arg->last_pulse = now;
  if (discard)
    return;

  PulseCounterCountMode mode = arg->isr_pin->digital_read() ? arg->rising_edge_mode : arg->falling_edge_mode;
  switch (mode) {
    case PULSE_COUNTER_DISABLE:
      break;
    case PULSE_COUNTER_INCREMENT:
      arg->counter++;
      break;
    case PULSE_COUNTER_DECREMENT:
      arg->counter--;
      break;
  }
}
void InterruptPulseCounterStorage::pulse_counter_setup(GPIOPin *pin) {
  this->pin = pin;
  this->pin->setup();
  this->isr_pin = this->pin->to_isr();
  this->pin->attach_interrupt(InterruptPulseCounterStorage::gpio_intr, this, CHANGE);
}

pulse_counter_t InterruptPulseCounterStorage::read_raw_value() {
  pulse_counter_t counter = this->counter;
  pulse_counter_t ret = counter - this->last_value;
  this->last_value = counter;
  return ret;
}

void InterruptPulseCounterSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up interrupt pulse counter '%s'...", this->name_.c_str());

  this->storage_.pulse_counter_setup(this->pin_);
}

void InterruptPulseCounterSensor::dump_config() {
  LOG_SENSOR("", "Interrupt Pulse Counter", this);
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Rising Edge: %s", EDGE_MODE_TO_STRING[this->storage_.rising_edge_mode]);
  ESP_LOGCONFIG(TAG, "  Falling Edge: %s", EDGE_MODE_TO_STRING[this->storage_.falling_edge_mode]);
  ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %u µs", this->storage_.filter_us);
  LOG_UPDATE_INTERVAL(this);
}

void InterruptPulseCounterSensor::update() {
  pulse_counter_t raw = this->storage_.read_raw_value();
  float value = (60000.0f * raw) / float(this->get_update_interval());  // per minute

  ESP_LOGD(TAG, "'%s': Retrieved counter: %0.2f pulses/min", this->get_name().c_str(), value);
  this->publish_state(value);

  if (this->total_sensor_ != nullptr) {
    current_total_ += raw;
    ESP_LOGD(TAG, "'%s': Total : %i pulses", this->get_name().c_str(), current_total_);
    this->total_sensor_->publish_state(current_total_);
  }
}

}  // namespace interrupt_pulse_counter
}  // namespace esphome
