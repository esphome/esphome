#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::emc230x {

static const uint8_t MAX_FANS = 5;

// Enum listing all supported EMC230X models.
enum Emc230xModel { EMC2301, EMC2302, EMC2303, EMC2305 };

/** Enum listing all PWM frequencies supported by the EMC230X family.
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

/** Enum listing all update time options supported by the EMC230X family.
 *
 * Specific values of the enum correspond to the update time bits from the fan configuration registers.
 */
enum Emc230xUpdateTime {
  EMC230X_UPDATE_TIME_100MS,
  EMC230X_UPDATE_TIME_200MS,
  EMC230X_UPDATE_TIME_300MS,
  EMC230X_UPDATE_TIME_400MS,
  EMC230X_UPDATE_TIME_500MS,
  EMC230X_UPDATE_TIME_800MS,
  EMC230X_UPDATE_TIME_1200MS,
  EMC230X_UPDATE_TIME_1600MS,
};

/** Enum listing all spin-up levels supported by the EMC230X family.
 *
 * Specific values of the enum correspond to the spin-up level bits from the fan spin-up configuration registers.
 */
enum Emc230xSpinUpLevel {
  EMC230X_SPIN_UP_LEVEL_30,
  EMC230X_SPIN_UP_LEVEL_35,
  EMC230X_SPIN_UP_LEVEL_40,
  EMC230X_SPIN_UP_LEVEL_45,
  EMC230X_SPIN_UP_LEVEL_50,
  EMC230X_SPIN_UP_LEVEL_55,
  EMC230X_SPIN_UP_LEVEL_60,
  EMC230X_SPIN_UP_LEVEL_65,
};

/** Enum listing all spin-up times supported by the EMC230X family.
 *
 * Specific values of the enum correspond to the spin-up time bits from the fan spin-up configuration registers.
 */
enum Emc230xSpinUpTime {
  EMC230X_SPIN_UP_TIME_250MS,
  EMC230X_SPIN_UP_TIME_500MS,
  EMC230X_SPIN_UP_TIME_1S,
  EMC230X_SPIN_UP_TIME_2S,
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

  /** Sets the PWM output type for a given fan
   *
   * @param fan The fan number
   * @param push_pull Whether to use push-pull (true) or open-drain (false) PWM output for this fan
   */
  void set_pwm_push_pull(uint8_t fan, bool push_pull) { this->pwm_push_pull_[fan - 1] = push_pull; }

  /** Sets the PWM frequency for a given fan
   *
   * @param fan The fan number
   * @param frequency The PWM frequency to use for this fan
   */
  void set_pwm_frequency(uint8_t fan, Emc230xPwmFrequency frequency) { this->pwm_frequencies_[fan - 1] = frequency; }

  /** Sets the PWM divider for a given fan
   *
   * @param fan The fan number
   * @param divider The PWM divider to use for this fan (0-255)
   */
  void set_pwm_divider(uint8_t fan, uint8_t divider) { this->pwm_dividers_[fan - 1] = divider; }

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

  /** Sets the update time for a given fan
   *
   * @param fan The fan number
   * @param update_time The update time to use for this fan
   */
  void set_update_time(uint8_t fan, Emc230xUpdateTime update_time) { this->update_times_[fan - 1] = update_time; }

  /** Sets the maximum step size for ramp rate control for a given fan
   *
   * @param fan The fan number
   * @param max_step_size The maximum step size to use for this fan (0-31). Set to 0 to disable ramp rate control.
   */
  void set_max_step_size(uint8_t fan, uint8_t max_step_size) { this->max_step_sizes_[fan - 1] = max_step_size; }

  /** Sets whether to enable the noise filter for a given fan tachometer reading
   *
   * @param fan The fan number
   * @param noise_filter Whether to enable the noise filter for this fan
   */
  void set_noise_filter(uint8_t fan, bool noise_filter) { this->noise_filters_[fan - 1] = noise_filter; }

  /** Sets whether to enable the spin-up 100% drive kick for a given fan
   *
   * @param fan The fan number
   * @param spin_up_kick Whether to enable the spin-up kick for this fan
   */
  void set_spin_up_kick(uint8_t fan, bool spin_up_kick) { this->spin_up_kick_[fan - 1] = spin_up_kick; }

  /** Sets the spin-up level for a given fan
   *
   * @param fan The fan number
   * @param spin_up_level The spin-up level to use for this fan
   */
  void set_spin_up_level(uint8_t fan, Emc230xSpinUpLevel spin_up_level) {
    this->spin_up_levels_[fan - 1] = spin_up_level;
  }

  /** Sets the spin-up time for a given fan
   *
   * @param fan The fan number
   * @param spin_up_time The spin-up time to use for this fan
   */
  void set_spin_up_time(uint8_t fan, Emc230xSpinUpTime spin_up_time) { this->spin_up_times_[fan - 1] = spin_up_time; }

  /** Returns whether the given fan number is valid for the detected EMC230X model
   * Logs a warning if the fan number is invalid
   *
   * @param fan The fan number to check
   * @return true if the fan number is valid for the detected model, false otherwise
   */
  bool is_valid_fan_number(uint8_t fan) const;

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
  std::array<bool, MAX_FANS> pwm_push_pull_{};
  std::array<Emc230xPwmFrequency, MAX_FANS> pwm_frequencies_{};
  std::array<uint8_t, MAX_FANS> pwm_dividers_{};
  std::array<uint8_t, MAX_FANS> pulses_per_revolution_{};
  std::array<Emc230xMinSpeedMeasurement, MAX_FANS> min_speed_measurements_{};
  std::array<Emc230xUpdateTime, MAX_FANS> update_times_{};
  std::array<uint8_t, MAX_FANS> max_step_sizes_{};
  std::array<bool, MAX_FANS> noise_filters_{};
  std::array<bool, MAX_FANS> spin_up_kick_{};
  std::array<Emc230xSpinUpLevel, MAX_FANS> spin_up_levels_ = {};
  std::array<Emc230xSpinUpTime, MAX_FANS> spin_up_times_ = {};
  // Pre-calculated conversion constants for tachometer reading to RPM for each fan based on the configuration
  std::array<uint32_t, MAX_FANS> rpm_conversion_constants_{};
};

}  // namespace esphome::emc230x
