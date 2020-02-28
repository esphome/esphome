#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace dimmer {

struct DimmerDataStore {
  /// Zero-cross pin
  ISRInternalGPIOPin *zero_cross_pin;
  /// Zero-cross pin number - used to share ZC pin across multiple dimmers
  uint8_t zero_cross_pin_number;
  /// Output pin to write to
  ISRInternalGPIOPin *gate_pin;
  /// Value of the dimmer - 0 to 65535.
  uint16_t value;
  /// Minimum power for activation
  uint16_t min_power;
  /// Time between the last two ZC pulses
  uint32_t cycle_time_us;
  /// Time (in micros()) of last ZC signal
  uint32_t crossed_zero_at;
  /// Time since last ZC pulse to enable gate pin. 0 means not set.
  uint32_t enable_time_us;

  uint32_t timer_intr(uint32_t now);

  void gpio_intr();
  static void s_gpio_intr(DimmerDataStore *store);
#ifdef ARDUINO_ARCH_ESP32
  static void s_timer_intr();
#endif
};

class Dimmer : public output::FloatOutput, public Component {
 public:
  void setup() override;

  void dump_config() override;
  void set_gate_pin(GPIOPin *gate_pin) { gate_pin_ = gate_pin; }
  void set_zero_cross_pin(GPIOPin *zero_cross_pin) { zero_cross_pin_ = zero_cross_pin; }

 protected:
  void write_state(float state) override;

  GPIOPin *gate_pin_;
  GPIOPin *zero_cross_pin_;
  DimmerDataStore store_;
};

}  // namespace dimmer
}  // namespace esphome
