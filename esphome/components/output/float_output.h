#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "binary_output.h"

namespace esphome::output {

#ifdef USE_OUTPUT_FLOAT_POWER_SCALING
#define LOG_FLOAT_OUTPUT(this) \
  LOG_BINARY_OUTPUT(this) \
  if (this->max_power_ != 1.0f) { \
    ESP_LOGCONFIG(TAG, "  Max Power: %.1f%%", this->max_power_ * 100.0f); \
  } \
  if (this->min_power_ != 0.0f) { \
    ESP_LOGCONFIG(TAG, "  Min Power: %.1f%%", this->min_power_ * 100.0f); \
  }
#else
#define LOG_FLOAT_OUTPUT(this) LOG_BINARY_OUTPUT(this)
#endif

/** Base class for all output components that can output a variable level, like PWM.
 *
 * Floating Point Outputs always use output values in the range from 0.0 to 1.0 (inclusive), where 0.0 means off
 * and 1.0 means fully on. While using floating point numbers might make computation slower, it
 * makes using maths much easier and (in theory) supports all possible bit depths.
 *
 * If you want to create a FloatOutput yourself, you essentially just have to override write_state(float).
 * That method will be called for you with inversion already applied. When USE_OUTPUT_FLOAT_POWER_SCALING is
 * enabled (set automatically by Python codegen if any output uses min_power/max_power/zero_means_zero or the
 * matching runtime actions), the value will additionally have max-min power scaling and offset to min_power
 * applied; otherwise only inversion is applied.
 *
 * This interface is compatible with BinaryOutput (and will automatically convert the binary states to floating
 * point states for you). Additionally, this class provides a way for users to set a minimum and/or maximum power
 * output (gated on USE_OUTPUT_FLOAT_POWER_SCALING).
 */
class FloatOutput : public BinaryOutput {
 public:
#ifdef USE_OUTPUT_FLOAT_POWER_SCALING
  /** Set the maximum power output of this component.
   *
   * All values are multiplied by max_power - min_power and offset to min_power to get the adjusted value.
   *
   * @param max_power Automatically clamped from 0 or min_power to 1.
   */
  void set_max_power(float max_power);

  /** Set the minimum power output of this component.
   *
   * All values are multiplied by max_power - min_power and offset by min_power to get the adjusted value.
   *
   * @param min_power Automatically clamped from 0 to max_power or 1.
   */
  void set_min_power(float min_power);

  /** Sets this output to ignore min_power for a 0 state
   *
   * @param zero_means_zero True if a 0 state should mean 0 and not min_power.
   */
  void set_zero_means_zero(bool zero_means_zero) { this->zero_means_zero_ = zero_means_zero; }
#else
  // Compile-time guards for users calling these methods from lambdas (documented usage at
  // https://esphome.io/components/output/#output-set_min_power_action). When power scaling
  // is compiled out, these template stubs fail to compile with an actionable error pointing
  // at the user's lambda. Templating on a default-false bool means static_assert only fires
  // on instantiation (i.e. when the user actually calls the method), not on every parse.
  template<bool _use_output_float_power_scaling = false> void set_max_power(float max_power) {
    static_assert(_use_output_float_power_scaling,
                  "set_max_power() requires USE_OUTPUT_FLOAT_POWER_SCALING. "
                  "To enable it, add 'max_power: 100%' (or any value) to one output entry in your YAML — "
                  "the codegen will then keep the scaling fields. "
                  "See https://esphome.io/components/output/ for details.");
  }
  template<bool _use_output_float_power_scaling = false> void set_min_power(float min_power) {
    static_assert(_use_output_float_power_scaling,
                  "set_min_power() requires USE_OUTPUT_FLOAT_POWER_SCALING. "
                  "To enable it, add 'min_power: 0%' (or any value) to one output entry in your YAML — "
                  "the codegen will then keep the scaling fields. "
                  "See https://esphome.io/components/output/ for details.");
  }
  template<bool _use_output_float_power_scaling = false> void set_zero_means_zero(bool zero_means_zero) {
    static_assert(_use_output_float_power_scaling,
                  "set_zero_means_zero() requires USE_OUTPUT_FLOAT_POWER_SCALING. "
                  "To enable it, add 'zero_means_zero: true' to one output entry in your YAML.");
  }
#endif

  /** Set the level of this float output, this is called from the front-end.
   *
   * @param state The new state.
   */
  void set_level(float state);

  /** Set the frequency of the output for PWM outputs.
   *
   * Implemented only by components which can set the output PWM frequency.
   *
   * @param frequence The new frequency.
   */
  virtual void update_frequency(float frequency) {}

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

#ifdef USE_OUTPUT_FLOAT_POWER_SCALING
  /// Get the maximum power output.
  float get_max_power() const { return this->max_power_; }

  /// Get the minimum power output.
  float get_min_power() const { return this->min_power_; }
#else
  /// Get the maximum power output.
  float get_max_power() const { return 1.0f; }

  /// Get the minimum power output.
  float get_min_power() const { return 0.0f; }
#endif

 protected:
  /// Implement BinarySensor's write_enabled; this should never be called.
  void write_state(bool state) override;
  virtual void write_state(float state) = 0;

#ifdef USE_OUTPUT_FLOAT_POWER_SCALING
  float max_power_{1.0f};
  float min_power_{0.0f};
  bool zero_means_zero_{false};
#endif
};

}  // namespace esphome::output
