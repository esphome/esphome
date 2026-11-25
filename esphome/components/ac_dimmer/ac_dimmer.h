#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome::ac_dimmer {

enum DimMethod : uint8_t { DIM_METHOD_LEADING_PULSE = 0, DIM_METHOD_LEADING, DIM_METHOD_TRAILING };

/// Zero-cross phase lock states
enum ZeroCrossPhase : uint8_t {
  ZERO_CROSS_PHASE_NONE = 0,
  ZERO_CROSS_PHASE_ONE_EDGE,
  ZERO_CROSS_PHASE_CONFIRMING,
  ZERO_CROSS_PHASE_LOCKED,
};

struct AcDimmerDataStore {
  /// Output pin to write to
  ISRInternalGPIOPin gate_pin;
  /// Start of the current half cycle (micros()); while acquiring, the last edge taken
  uint32_t zc_at;
  /// Length of a half cycle in µs; while acquiring, the last gap between taken edges
  uint16_t period;
  /// Time after zc_at to switch the gate on, 0 for none; while acquiring, the latest full cycle (0 for none)
  uint16_t enable_time_us;
  /// Time after zc_at to switch the gate off, 0 for none; while acquiring, the full cycle before it (0 for none)
  uint16_t disable_time_us;
  /// Value of the dimmer, 0 to 65535
  uint16_t value;
  /// Minimum power in 0.1 % steps
  uint16_t min_power;
  /// Timing error of the best edge of the latest crossing with an edge, not applied yet; NO_EDGE_ERR for none
  int16_t taken_err;
  /// Zero-cross pin number, used to share the pin across dimmers
  uint8_t zero_cross_pin_number;
  /// ZeroCrossPhase
  uint8_t phase : 2;
  /// Half cycles in a row without an edge in their window
  uint8_t misses : 4;
  /// An edge was taken for the crossing at zc_at
  uint8_t edge_now : 1;
  /// An edge was taken for the crossing at zc_at + period
  uint8_t edge_next : 1;
  /// Set to send the first half cycle complete
  bool init_cycle;
  /// Dimmer method
  DimMethod method;
  /// Edges followed by only `close` while far early, each undone by a late edge (capped)
  uint8_t early_steps : 3;
  /// The edge of the last crossing with one was late beyond `close`
  uint8_t late_last : 1;
  /// The same for the crossing with an edge before it
  uint8_t late_before : 1;
  /// The last edge was a lone late one and counted as `close`
  uint8_t late_step : 1;

  /// Timer work at `now`: starts half cycles and drives the gate; returns µs until the next event
  uint32_t timer_intr(uint32_t now);
  /// timer_intr() work for a locked dimmer, called with the timing lock held
  uint32_t timer_step(uint32_t now);
  /// Zero-cross edge at `now`: acquires the lock or corrects the timing
  void zero_cross_edge(uint32_t now);
  /// Edge while not locked: locks once three full cycles in a row agree
  void acquire_edge(uint32_t now);
  /// Edge while locked: an edge near the predicted crossing nudges the timing, others are ignored
  void track_edge(uint32_t now);
  /// Moves the timing by the waiting edge error
  void apply_edge_err();
  /// Sets up the gate events of the half cycle that starts at zc_at
  void start_half_cycle();

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
