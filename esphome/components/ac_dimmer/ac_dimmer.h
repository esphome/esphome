#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome::ac_dimmer {

enum DimMethod : uint8_t { DIM_METHOD_LEADING_PULSE = 0, DIM_METHOD_LEADING, DIM_METHOD_TRAILING };

struct AcDimmerDataStore {
  /// Output pin to write to
  ISRInternalGPIOPin gate_pin;
  /// Length of a half cycle, measured between zero-cross edges and then following them slowly
  uint32_t cycle_time_us;
  /// Time (in micros()) of the last zero crossing; an expected one can be a little ahead (see take_zero_cross_edge)
  uint32_t crossed_zero_at;
  /// The zero crossing the gate events below are counted from
  uint32_t schedule_zero_at;
  /// Time since last ZC pulse to enable gate pin. 0 means not set.
  uint32_t enable_time_us;
  /// Time since last ZC pulse to disable gate pin. 0 means no disable.
  uint32_t disable_time_us;
  /// Value of the dimmer - 0 to 65535.
  uint16_t value;
  /// Minimum power for activation
  uint16_t min_power;
  /// Zero-cross pin number - used to share ZC pin across multiple dimmers
  uint8_t zero_cross_pin_number;
  /// Zero-cross edges in a row that did not fit the measured timing; too many and it is measured again
  uint8_t doubtful_edges{0};
  /// Edges that confirmed a new measurement of the half cycle; gate events start when it is confirmed
  uint8_t confirmed_edges{0};
  /// The last crossing was taken from an edge that came before its expected time
  bool crossing_early{false};
  /// Set to send the first half ac cycle complete
  bool init_cycle;
  /// Dimmer method
  DimMethod method;

  uint32_t timer_intr(uint32_t now);

  /// Handles a zero-cross edge at `now`; returns true if it starts a new half cycle
  bool take_zero_cross_edge(uint32_t now);
  /// True once a measured half cycle has been confirmed by the edges after it
  bool timing_locked() const;
  /// Starts measuring the half cycle again, from an edge at `now`
  void restart_timing(uint32_t now);
  /// Zero-cross edge at `now`: tracks the timing and starts the half cycle, now or at the expected crossing
  void zero_cross_edge(uint32_t now);
  /// Sets up the gate events of the half cycle that starts at crossed_zero_at
  void start_half_cycle();
  /// Drops pending gate events and switches a partly-on output off
  void cancel_schedule();

  void gpio_intr();
  static void s_gpio_intr(AcDimmerDataStore *store);
#ifdef USE_ESP32
  static void s_timer_intr();
#endif
};

class AcDimmer final : public output::FloatOutput, public Component {
 public:
  void setup() override;

  void dump_config() override;
  void set_gate_pin(InternalGPIOPin *gate_pin) { gate_pin_ = gate_pin; }
  void set_zero_cross_pin(InternalGPIOPin *zero_cross_pin) { zero_cross_pin_ = zero_cross_pin; }
  void set_zero_cross_interrupt_type(gpio::InterruptType type) { zero_cross_interrupt_type_ = type; }
  void set_init_with_half_cycle(bool init_with_half_cycle) { init_with_half_cycle_ = init_with_half_cycle; }
  void set_method(DimMethod method) { this->store_.method = method; }

 protected:
  void write_state(float state) override;

  InternalGPIOPin *gate_pin_;
  InternalGPIOPin *zero_cross_pin_;
  AcDimmerDataStore store_;
  gpio::InterruptType zero_cross_interrupt_type_;
  bool init_with_half_cycle_;
};

}  // namespace esphome::ac_dimmer
