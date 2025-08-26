#include "gpio_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gpio {

static const char *const TAG = "gpio.binary_sensor";

void IRAM_ATTR GPIOBinarySensorStore::gpio_intr(GPIOBinarySensorStore *arg) {
  bool new_state = arg->isr_pin_.digital_read();
  if (new_state != arg->last_state_) {
    arg->state_ = new_state;
    arg->last_state_ = new_state;
    arg->changed_ = true;
    // Wake up the component from its disabled loop state
    if (arg->component_ != nullptr) {
      arg->component_->enable_loop_soon_any_context();
    }
  }
}

void GPIOBinarySensorStore::setup(InternalGPIOPin *pin, gpio::InterruptType type, Component *component) {
  pin->setup();
  this->isr_pin_ = pin->to_isr();
  this->component_ = component;

  // Read initial state
  this->last_state_ = pin->digital_read();
  this->state_ = this->last_state_;

  // Attach interrupt - from this point on, any changes will be caught by the interrupt
  pin->attach_interrupt(&GPIOBinarySensorStore::gpio_intr, this, type);
}

void GPIOBinarySensor::setup() {
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
  const char *mode = this->use_interrupt_ ? "interrupt" : "polling";
  ESP_LOGCONFIG(TAG, "  Mode: %s", mode);
  if (this->use_interrupt_) {
    const char *interrupt_type;
    switch (this->interrupt_type_) {
      case gpio::INTERRUPT_RISING_EDGE:
        interrupt_type = "RISING_EDGE";
        break;
      case gpio::INTERRUPT_FALLING_EDGE:
        interrupt_type = "FALLING_EDGE";
        break;
      case gpio::INTERRUPT_ANY_EDGE:
        interrupt_type = "ANY_EDGE";
        break;
      default:
        interrupt_type = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Interrupt Type: %s", interrupt_type);
  }
}

void GPIOBinarySensor::loop() {
  if (this->use_interrupt_) {
    if (this->store_.is_changed()) {
      // Clear the flag immediately to minimize the window where we might miss changes
      this->store_.clear_changed();
      // Read the state and publish it
      // Note: If the ISR fires between clear_changed() and get_state(), that's fine -
      // we'll process the new change on the next loop iteration
      bool state = this->store_.get_state();
      this->publish_state(state);
    } else {
      // No changes, disable the loop until the next interrupt
      this->disable_loop();
    }
  } else {
    this->publish_state(this->pin_->digital_read());
  }
}

float GPIOBinarySensor::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace gpio
}  // namespace esphome
