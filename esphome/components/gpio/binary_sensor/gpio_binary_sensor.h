#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace gpio {

// Store class for ISR data (no vtables, ISR-safe)
class GPIOBinarySensorStore {
 public:
  void setup(InternalGPIOPin *pin, gpio::InterruptType type, Component *component);

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

 protected:
  ISRInternalGPIOPin isr_pin_;
  volatile bool state_{false};
  volatile bool last_state_{false};
  volatile bool changed_{false};
  Component *component_{nullptr};  // Pointer to the component for enable_loop_soon_any_context()
};

class GPIOBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  // No destructor needed: ESPHome components are created at boot and live forever.
  // Interrupts are only detached on reboot when memory is cleared anyway.

  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_use_interrupt(bool use_interrupt) { use_interrupt_ = use_interrupt; }
  void set_interrupt_type(gpio::InterruptType type) { interrupt_type_ = type; }
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
  bool use_interrupt_{true};
  gpio::InterruptType interrupt_type_{gpio::INTERRUPT_ANY_EDGE};
  GPIOBinarySensorStore store_;
};

}  // namespace gpio
}  // namespace esphome
