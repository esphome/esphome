#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::emc2303 {

/** Enum listing all PWM frequencys supported by the EMC2303.
 *
 * Specific values of the enum correspond to the PWM frequency register settings from the EMC2303 datasheet
 */
enum Emc2303PwmFrequency {
  EMC2303_PWM_FREQUENCY_26000HZ,
  EMC2303_PWM_FREQUENCY_19531HZ,
  EMC2303_PWM_FREQUENCY_4882HZ,
  EMC2303_PWM_FREQUENCY_2441HZ,
};

/** Enum listing all minimum speed measurement options supported by the EMC2303.
 *
 * Specific values of the enum correspond to the RANGE bit settings from the EMC2303 datasheet
 */
enum Emc2303MinSpeedMeasurement {
  EMC2303_MIN_SPEED_500RPM,
  EMC2303_MIN_SPEED_1000RPM,
  EMC2303_MIN_SPEED_2000RPM,
  EMC2303_MIN_SPEED_4000RPM,
};

// This class includes support for the EMC2303 i2c fan controller.
// The device has three outputs and several sensors and this
// class is for the EMC2303 configuration.
class Emc2303Component : public Component, public i2c::I2CDevice {
 public:
  /** Configures the continuous watchdog timer
   *
   * @param watchdog Enables or disables the watchdog timer
   */
  void set_watchdog(bool watchdog) { this->watchdog_ = watchdog; }

  /** Sets the PWM frequency for a given fan
   *
   * @param fan The fan number
   * @param frequency The PWM frequency to use for this fan
   */
  void set_pwm_frequency(uint8_t fan, Emc2303PwmFrequency frequency) { this->pwm_frequencies_[fan - 1] = frequency; }

  /** Sets the number of pulses per revolution for a given fan
   *
   * @param fan The fan number
   * @param pulses The number of pulses per revolution for the fan
   */
  void set_pulses_per_revolution(uint8_t fan, uint8_t pulses) { this->pulses_per_revolution_[fan - 1] = pulses; }

  /** Sets the minimum speed measurement for a given fan
   *
   * @param fan The fan number
   * @param speed The minimum speed measurement to use for this fan
   */
  void set_min_speed_measurement(uint8_t fan, Emc2303MinSpeedMeasurement speed) {
    this->min_speed_measurements_[fan - 1] = speed;
  }

  /** Sets the duty cycle for a given fan.
   *
   * @param fan The fan number
   * @param value The duty cycle value, from 0.0f to 1.0f
   */
  void set_duty_cycle(uint8_t fan, float value);

  /** Gets the tachometer speed sensor reading of a given fan in RPM.
   *
   * @param fan The fan number
   * @return The fan speed in RPM
   */
  float get_speed(uint8_t fan);

  /** Used by ESPHome framework. */
  void setup() override;
  /** Used by ESPHome framework. */
  void dump_config() override;
  /** Used by ESPHome framework. */
  float get_setup_priority() const override;

 protected:
  bool watchdog_{false};
  std::array<Emc2303PwmFrequency, 3> pwm_frequencies_{};
  std::array<uint8_t, 3> pulses_per_revolution_{};
  std::array<Emc2303MinSpeedMeasurement, 3> min_speed_measurements_{};
  std::array<float, 3> rpm_conversion_constants_{};
};

}  // namespace esphome::emc2303
