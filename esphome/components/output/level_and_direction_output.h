#pragma once

namespace esphome {
namespace output {

/** Interface for an output with level (0–1) and reverse.
 *
 * Fits a level pin (e.g. PWM) and a direction/reverse pin on a motor controller
 * (e.g. for a Peltier module).
 */
class LevelAndDirectionOutput {
 public:
  /** Set the output level, 0 = off, 1 = full. Use with a single 0–1 output
   * (e.g. PWM); no separate output type needed.
   *
   * @param level Value in [0, 1].
   */
  virtual void set_level(float level) = 0;

  /** Set reverse. true = reverse direction (e.g. cool), false = forward
   * (e.g. heat). Typically drives a direction pin on the motor controller.
   */
  virtual void set_reverse(bool reverse) = 0;
};

}  // namespace output
}  // namespace esphome
