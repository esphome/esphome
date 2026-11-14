#include "gpio_binary_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::gpio {

ESPHOME_LOG_TAG(TAG, "gpio.binary_sensor");

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
// Interrupt type strings indexed by edge-triggered InterruptType values:
// indices 1-3: RISING_EDGE, FALLING_EDGE, ANY_EDGE; other values (e.g. level-triggered) map to UNKNOWN (index 0).
PROGMEM_STRING_TABLE(InterruptTypeStrings, "UNKNOWN", "RISING_EDGE", "FALLING_EDGE", "ANY_EDGE");

static const LogString *interrupt_type_to_string(gpio::InterruptType type) {
  return InterruptTypeStrings::get_log_str(static_cast<uint8_t>(type), 0);
}
#endif
#endif

#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
void IRAM_ATTR GPIOBinarySensor::gpio_intr(GPIOBinarySensor *arg) {
  GPIOBinarySensorStore &store = arg->store_;
  bool new_state = store.isr_pin_.digital_read();
  if (new_state != store.state_) {
    store.state_ = new_state;
    store.changed_ = true;
    // Wake up the component from its disabled loop state
    arg->enable_loop_soon_any_context();
  }
}

void GPIOBinarySensorStore::setup(InternalGPIOPin *pin) {
  pin->setup();
  this->isr_pin_ = pin->to_isr();
  this->state_ = pin->digital_read();
}
#endif  // USE_GPIO_BINARY_SENSOR_INTERRUPT

void GPIOBinarySensor::setup() {
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  if (this->store_.use_interrupt_) {
    auto *internal_pin = static_cast<InternalGPIOPin *>(this->pin_);
    this->store_.setup(internal_pin);
    // Attach after reading the initial state, from here on every change is caught by the interrupt
    internal_pin->attach_interrupt(&GPIOBinarySensor::gpio_intr, this, this->store_.interrupt_type_);
    this->publish_initial_state(this->store_.get_state());
    return;
  }
#endif
  this->pin_->setup();
  this->publish_initial_state(this->pin_->digital_read());
}

void GPIOBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "GPIO Binary Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  if (this->store_.use_interrupt_) {
    ESP_LOGCONFIG(TAG,
                  "  Mode: interrupt\n"
                  "  Interrupt Type: %s",
                  LOG_STR_ARG(interrupt_type_to_string(this->store_.interrupt_type_)));
    return;
  }
#endif
  ESP_LOGCONFIG(TAG, "  Mode: polling");
}

void GPIOBinarySensor::loop() {
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  if (this->store_.use_interrupt_) {
    if (this->store_.is_changed()) {
      // Clear first: an ISR firing before get_state() is then handled on the next loop
      this->store_.clear_changed();
      this->publish_state(this->store_.get_state());
    } else {
      // No changes, disable the loop until the next interrupt
      this->disable_loop();
    }
    return;
  }
#endif
  this->publish_state(this->pin_->digital_read());
}

float GPIOBinarySensor::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esphome::gpio
