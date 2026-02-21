#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::emc230x {

static const uint8_t MAX_FANS = 5;

// Enum listing all supported EMC230X models.
enum Emc230xModel { EMC2301, EMC2302, EMC2303, EMC2305 };

/** Enum listing all PWM frequencys supported by the EMC230X family.
 *
 * Specific values of the enum correspond to the PWM frequency register settings from the EMC230X datasheet
 */
enum Emc230xPwmFrequency {
  EMC230X_PWM_FREQUENCY_26000HZ,
  EMC230X_PWM_FREQUENCY_19531HZ,
  EMC230X_PWM_FREQUENCY_4882HZ,
  EMC230X_PWM_FREQUENCY_2441HZ,
};

/** Enum listing all minimum speed measurement options supported by the EMC230X family.
 *
 * Specific values of the enum correspond to the RANGE bit settings from the EMC230X datasheet
 */
enum Emc230xMinSpeedMeasurement {
  EMC230X_MIN_SPEED_500RPM,
  EMC230X_MIN_SPEED_1000RPM,
  EMC230X_MIN_SPEED_2000RPM,
  EMC230X_MIN_SPEED_4000RPM,
};

// This class includes support for the EMC230X i2c fan controller family, which includes
// the EMC2301, EMC2302, EMC2303, and EMC2305 models.
// These models support 1, 2, 3, and 5 fans respectively
class Emc230xComponent : public Component, public i2c::I2CDevice {
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
  void set_pwm_frequency(uint8_t fan, Emc230xPwmFrequency frequency) { this->pwm_frequencies_[fan - 1] = frequency; }

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
  void set_min_speed_measurement(uint8_t fan, Emc230xMinSpeedMeasurement speed) {
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
  Emc230xModel emc230x_model_{EMC2301};
  uint8_t fan_count_{0};

  // EMC230X configuration options
  bool watchdog_{false};

  // Fan-specific configuration options
  std::array<Emc230xPwmFrequency, MAX_FANS> pwm_frequencies_{};
  std::array<uint8_t, MAX_FANS> pulses_per_revolution_{};
  std::array<Emc230xMinSpeedMeasurement, MAX_FANS> min_speed_measurements_{};
  // Pre-calculated conversion constants for tachometer reading to RPM for each fan based on the configuration
  std::array<uint32_t, MAX_FANS> rpm_conversion_constants_{};
};

}  // namespace esphome::emc230x
