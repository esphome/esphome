#pragma once

#include "esphome/core/component.h"
#include "binary_output.h"

namespace esphome {
namespace output {

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
 * point states for you).
 */
class FloatOutput : public BinaryOutput {
 public:
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

 protected:
  /// Implement BinarySensor's write_enabled; this should never be called.
  void write_state(bool state) override;
  virtual void write_state(float state) = 0;
};

}  // namespace output
}  // namespace esphome
