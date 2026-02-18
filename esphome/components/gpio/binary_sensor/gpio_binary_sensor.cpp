#include "gpio_binary_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome {
namespace deep_sleep {
extern uint32_t get_wakeup_pin();
}

namespace gpio {

static const char *const TAG = "gpio.binary_sensor";

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
static const LogString *interrupt_type_to_string(gpio::InterruptType type) {
  return LOG_STR("INTERRUPT");
}

static const LogString *gpio_mode_to_string(bool use_interrupt) {
  return use_interrupt ? LOG_STR("interrupt") : LOG_STR("polling");
}
#endif

void IRAM_ATTR GPIOBinarySensorStore::gpio_intr(GPIOBinarySensorStore *arg) {
  bool new_state = arg->isr_pin_.digital_read();
  if (new_state != arg->last_state_) {
    arg->state_ = new_state;
    arg->last_state_ = new_state;
    arg->changed_ = true;
    if (arg->component_ != nullptr) {
      arg->component_->enable_loop_soon_any_context();
    }
  }
}

void GPIOBinarySensorStore::setup(InternalGPIOPin *pin, gpio::InterruptType type, Component *component) {
  pin->setup();
  this->isr_pin_ = pin->to_isr();
  this->component_ = component;
  this->last_state_ = pin->digital_read();
  this->state_ = this->last_state_;
  pin->attach_interrupt(&GPIOBinarySensorStore::gpio_intr, this, type);
}

void GPIOBinarySensor::setup() {
  if (this->pin_->is_internal()) {
    auto *internal_pin = static_cast<InternalGPIOPin *>(this->pin_);
    uint32_t deep_sleep_pin = deep_sleep::get_wakeup_pin();
    uint32_t current_pin = internal_pin->get_pin();
    
    if (current_pin != deep_sleep_pin && deep_sleep_pin != UINT32_MAX) {
      this->use_interrupt_ = false;
    }
  }
  
  if (this->use_interrupt_ && !this->pin_->is_internal()) {
    ESP_LOGD(TAG, "GPIO is not internal, falling back to polling mode");
    this->use_interrupt_ = false;
  }

  if (this->use_interrupt_) {
    auto *internal_pin = static_cast<InternalGPIOPin *>(this->pin_);
    this->store_.setup(internal_pin, this->interrupt_type_, this);
    this->publish_initial_state(this->store_.get_state());
  } else {
    this->pin_->setup();
    this->publish_initial_state(this->pin_->digital_read());
  }
}

void GPIOBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "GPIO Binary Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Mode: %s", LOG_STR_ARG(gpio_mode_to_string(this->use_interrupt_)));
  if (this->use_interrupt_) {
    ESP_LOGCONFIG(TAG, "  Interrupt Type: %s", LOG_STR_ARG(interrupt_type_to_string(this->interrupt_type_)));
  }
}

void GPIOBinarySensor::loop() {
  if (this->use_interrupt_) {
    if (this->store_.is_changed()) {
      this->store_.clear_changed();
      bool state = this->store_.get_state();
      this->publish_state(state);
    } else {
      this->disable_loop();
    }
  } else {
    this->publish_state(this->pin_->digital_read());
  }
}

float GPIOBinarySensor::get_setup_priority() const { return setup_priority::LATE - 10.0f; }

}  // namespace gpio
}  // namespace esphome