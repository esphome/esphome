#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::gpio {

#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
// Store class for ISR data and configuration (no vtables, ISR-safe)
class GPIOBinarySensorStore {
 public:
  void setup(InternalGPIOPin *pin);

  // No lock needed: state_ is atomically updated by the ISR
  bool get_state() const { return this->state_; }
  bool is_changed() const { return this->changed_; }
  void clear_changed() { this->changed_ = false; }

 protected:
  friend class GPIOBinarySensor;
  ISRInternalGPIOPin isr_pin_;
  volatile bool state_{false};
  volatile bool changed_{false};
  gpio::InterruptType interrupt_type_{gpio::INTERRUPT_ANY_EDGE};
  bool use_interrupt_{true};
};
#endif

class GPIOBinarySensor final : public binary_sensor::BinarySensor, public Component {
 public:
  // User provided, not "= default": `new(p) GPIOBinarySensor()` would zero-fill .bss that is already zero.
  GPIOBinarySensor() {}

  // No destructor needed: ESPHome components are created at boot and live forever.
  // Interrupts are only detached on reboot when memory is cleared anyway.

  void set_pin(GPIOPin *pin) { this->pin_ = pin; }
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  void set_use_interrupt(bool use_interrupt) { this->store_.use_interrupt_ = use_interrupt; }
  void set_interrupt_type(gpio::InterruptType type) { this->store_.interrupt_type_ = type; }
#else
  // Polling-only build: codegen still emits set_use_interrupt(false) calls,
  // so keep the setter as an inlined no-op instead of storing the flag.
  void set_use_interrupt(bool /*use_interrupt*/) {}
#endif
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

 protected:
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  static void gpio_intr(GPIOBinarySensor *arg);
#endif

  GPIOPin *pin_{nullptr};
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  GPIOBinarySensorStore store_;
#endif
};

}  // namespace esphome::gpio
