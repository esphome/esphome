#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::gpio {

// Store class for ISR data and configuration (no vtables, ISR-safe)
class GPIOBinarySensorStore {
 public:
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  void setup(InternalGPIOPin *pin, Component *component);

  static void gpio_intr(GPIOBinarySensorStore *arg);

  bool get_state() const {
    // No lock needed: state_ is atomically updated by ISR
    // Volatile ensures we read the latest value
    return this->state_;
  }

  bool is_changed() const {
    // Simple read of volatile bool - no clearing here
    return this->changed_;
  }

  void clear_changed() {
    // Separate method to clear the flag
    this->changed_ = false;
  }
#endif

 protected:
  friend class GPIOBinarySensor;
#ifdef USE_GPIO_BINARY_SENSOR_INTERRUPT
  ISRInternalGPIOPin isr_pin_;
  Component *component_{nullptr};  // Pointer to the component for enable_loop_soon_any_context()
  volatile bool state_{false};
  volatile bool changed_{false};
  gpio::InterruptType interrupt_type_{gpio::INTERRUPT_ANY_EDGE};
  bool use_interrupt_{true};
#endif
};

class GPIOBinarySensor final : public binary_sensor::BinarySensor, public Component {
 public:
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
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup pin
  void setup() override;
  void dump_config() override;
  /// Hardware priority
  float get_setup_priority() const override;
  /// Check sensor
  void loop() override;

 protected:
  GPIOPin *pin_;
  GPIOBinarySensorStore store_;
};

}  // namespace esphome::gpio
