#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace ac_dimmer {
/// RB- I added a new method DS_MODULATOR_HALF and DS_MODULATOR_FULL
enum DimMethod { DIM_METHOD_LEADING_PULSE = 0, DIM_METHOD_LEADING, DIM_METHOD_TRAILING, DS_MODULATOR_HALF , DS_MODULATOR_FULL};

struct AcDimmerDataStore {
  /// Zero-cross pin
  ISRInternalGPIOPin zero_cross_pin;
  /// Zero-cross pin number - used to share ZC pin across multiple dimmers
  uint8_t zero_cross_pin_number;
  /// Output pin to write to
  ISRInternalGPIOPin gate_pin;
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
  /// Time since last ZC pulse to disable gate pin. 0 means no disable.
  uint32_t disable_time_us;
  /// Set to send the first half ac cycle complete
  bool init_cycle;
  /// Dimmer method
  DimMethod method;
  /// RB- ds modulator trigger
  bool ds_trigger;
  /// RB- integrator value for ds
  int32_t ds_integrator;
  /// RB- feedback value for ds
  uint16_t ds_feedback;
  /// RB- Flag to calculate modulation once per cycle
  bool ds_flag;
  /// RB- flag to determine odd pulses when DS_MODULATOR_HALF is selected
  bool ds_odd;

  uint32_t timer_intr(uint32_t now);

  void gpio_intr();
  static void s_gpio_intr(AcDimmerDataStore *store);
#ifdef USE_ESP32
  static void s_timer_intr();
#endif
};

class AcDimmer : public output::FloatOutput, public Component {
 public:
  void setup() override;

  void dump_config() override;
  void set_gate_pin(InternalGPIOPin *gate_pin) { gate_pin_ = gate_pin; }
  void set_zero_cross_pin(InternalGPIOPin *zero_cross_pin) { zero_cross_pin_ = zero_cross_pin; }
  void set_init_with_half_cycle(bool init_with_half_cycle) { init_with_half_cycle_ = init_with_half_cycle; }
  void set_method(DimMethod method) { method_ = method; }

 protected:
  void write_state(float state) override;

  InternalGPIOPin *gate_pin_;
  InternalGPIOPin *zero_cross_pin_;
  AcDimmerDataStore store_;
  bool init_with_half_cycle_;
  DimMethod method_;
  /// RB- for ds module initialization
  int32_t ds_integrator_;
  uint16_t ds_feedback_;
};

}  // namespace ac_dimmer
}  // namespace esphome

#endif  // USE_ARDUINO
