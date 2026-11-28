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
  /// Time after zc_at to switch the gate on, 0 for none; once on, when it went on; while acquiring, the latest full
  /// cycle (0 for none)
  uint16_t enable_time_us;
  /// Time after zc_at to switch the gate off, 0 for none; while acquiring, the full cycle before it (0 for none)
  uint16_t disable_time_us;
  /// Value of the dimmer, 0 to 65535
  uint16_t value;
  /// Minimum power in 0.1 % steps
  uint16_t min_power;
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
  /// The gate went on at enable_time_us in this half cycle
  uint8_t gate_fired : 1;
  /// Set to send the first half cycle complete; written from the main loop, so not in a bit field
  bool init_cycle;
  /// Edges followed by only `close` while far early, each undone by a late edge (capped)
  uint8_t early_steps : 3;
  /// Crossings in a row confirmed only by an edge right after a gate close to the crossing (capped)
  uint8_t gate_edges : 3;
  /// Dimmer method; set before setup(), so it can share the byte with the interrupt's flags
  DimMethod method : 2;

  /// Timer work at `now`: starts half cycles and drives the gate; returns µs until the next event
  uint32_t timer_intr(uint32_t now);
  /// timer_intr() work for a locked dimmer, called with the timing lock held
  uint32_t timer_step(uint32_t now);
  /// Whether an edge at `now` is noise from this dimmer's gate going on
  bool gate_noise_at(uint32_t now) const;
  /// Zero-cross edge at `now`: acquires the lock or corrects the timing. `noise_from` is the dimmer sharing the
  /// zero-cross input whose gate just went on (gate_noise_at()), nullptr for none.
  void zero_cross_edge(uint32_t now, const AcDimmerDataStore *noise_from);
  /// Edge while not locked: locks once three full cycles in a row agree
  void acquire_edge(uint32_t now);
  /// Edge while locked: the first edge near the predicted crossing nudges the timing, others are ignored
  void track_edge(uint32_t now, const AcDimmerDataStore *noise_from);
  /// Sets up the gate events of the half cycle that starts at zc_at
  void start_half_cycle();

  void gpio_intr(uint32_t now, const AcDimmerDataStore *noise_from);
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
