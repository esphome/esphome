#pragma once

#include "esphome/core/component.h"
#include "binary_output.h"

namespace esphome {
namespace output {

#define LOG_FLOAT_OUTPUT(this) \
  LOG_BINARY_OUTPUT(this) \
  if (this->max_power_ != 1.0f) { \
    ESP_LOGCONFIG(TAG, "  Max Power: %.1f%%", this->max_power_ * 100.0f); \
  } \
  if (this->min_power_ != 0.0f) { \
    ESP_LOGCONFIG(TAG, "  Min Power: %.1f%%", this->min_power_ * 100.0f); \
  }

/** Base class for all output components that can output a variable level, like PWM.
 *
 * Floating Point Outputs always use output values in the range from 0.0 to 1.0 (inclusive), where 0.0 means off
 * and 1.0 means fully on. While using floating point numbers might make computation slower, it
 * makes using maths much easier and (in theory) supports all possible bit depths.
 *
 * If you want to create a FloatOutput yourself, you essentially just have to override write_state(float).
 * That method will be called for you with inversion and max-min power and offset to min power already applied.
 *
 * This interface is compatible with BinaryOutput (and will automatically convert the binary states to floating
 * point states for you). Additionally, this class provides a way for users to set a minimum and/or maximum power
 * output
 */
class FloatOutput : public BinaryOutput {
 public:
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
   * @param zero True if a 0 state should mean 0 and not min_power.
   */
  void set_zero_means_zero(bool zero_means_zero);

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

  /// Get the maximum power output.
  float get_max_power() const;

  /// Get the minimum power output.
  float get_min_power() const;

 protected:
  /// Implement BinarySensor's write_enabled; this should never be called.
  void write_state(bool state) override;
  virtual void write_state(float state) = 0;

  float max_power_{1.0f};
  float min_power_{0.0f};
  bool zero_means_zero_;
};

}  // namespace output
}  // namespace esphome
