#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace emc2101 {

/** Enum listing all DAC conversion rates for the EMC2101.
 *
 * Specific values of the enum constants are register values taken from the EMC2101 datasheet.
 */
enum Emc2101DACConversionRate {
  EMC2101_DAC_1_EVERY_16_S,
  EMC2101_DAC_1_EVERY_8_S,
  EMC2101_DAC_1_EVERY_4_S,
  EMC2101_DAC_1_EVERY_2_S,
  EMC2101_DAC_1_EVERY_SECOND,
  EMC2101_DAC_2_EVERY_SECOND,
  EMC2101_DAC_4_EVERY_SECOND,
  EMC2101_DAC_8_EVERY_SECOND,
  EMC2101_DAC_16_EVERY_SECOND,
  EMC2101_DAC_32_EVERY_SECOND,
};

/// This class includes support for the EMC2101 i2c fan controller.
/// The device has an output (PWM or DAC) and several sensors and this
/// class is for the EMC2101 configuration.
class Emc2101Component : public Component, public i2c::I2CDevice {
 public:
  /** Sets the mode of the output.
   *
   * @param dac_mode false for PWM output and true for DAC mode.
   */
  void set_dac_mode(bool dac_mode) {
    this->dac_mode_ = dac_mode;
    this->max_output_value_ = 63;
  }

  /** Sets the PWM resolution.
   *
   * @param resolution the PWM resolution.
   */
  void set_pwm_resolution(uint8_t resolution) {
    this->pwm_resolution_ = resolution;
    this->max_output_value_ = 2 * resolution;
  }

  /** Sets the PWM divider used to derive the PWM frequency.
   *
   * @param divider The PWM divider.
   */
  void set_pwm_divider(uint8_t divider) { this->pwm_divider_ = divider; }

  /** Sets the DAC conversion rate (how many conversions per second).
   *
   * @param conversion_rate The DAC conversion rate.
   */
  void set_dac_conversion_rate(Emc2101DACConversionRate conversion_rate) {
    this->dac_conversion_rate_ = conversion_rate;
  }

  /** Inverts the polarity of the Fan output.
   *
   * @param inverted Invert or not the Fan output.
   */
  void set_inverted(bool inverted) { this->inverted_ = inverted; }

  /** Used by ESPHome framework. */
  void setup() override;
  /** Used by ESPHome framework. */
  void dump_config() override;
  /** Used by ESPHome framework. */
  float get_setup_priority() const override;

 protected:
  friend class EMC2101Output;
  friend class EMC2101Sensor;

  /** Sets the Fan output duty cycle
   *
   * @param value The duty cycle value, from 0.0f to 1.0f.
   */
  void set_duty_cycle_(float value);

  /** Gets the Fan output duty cycle percentage
   *
   * @return The duty cycle percentage from 0.0f to 100.0f.
   */
  float get_duty_cycle_percent_();

  /** Gets the internal temperature sensor reading.
   *
   * @return The temperature in degrees celsius.
   */
  float get_internal_temperature_();

  /** Gets the external temperature sensor reading.
   *
   * @return The temperature in degrees celsius.
   */
  float get_external_temperature_();

  /** Gets the tachometer speed sensor reading.
   *
   * @return The fan speed in RPMs.
   */
  float get_speed_();

  bool dac_mode_{false};
  bool inverted_{false};
  uint8_t max_output_value_;
  uint8_t pwm_resolution_;
  uint8_t pwm_divider_;
  Emc2101DACConversionRate dac_conversion_rate_;
};

/// This class allows to control the EMC2101 output.
class EMC2101Output : public output::FloatOutput {
 public:
  EMC2101Output(Emc2101Component *parent) : parent_(parent) {}

 protected:
  /** Used by ESPHome framework. */
  void write_state(float state) override;

  Emc2101Component *parent_;
};

/// This class exposes the EMC2101 sensors.
class EMC2101Sensor : public PollingComponent {
 public:
  EMC2101Sensor(Emc2101Component *parent) : parent_(parent) {}
  /** Used by ESPHome framework. */
  void dump_config() override;
  /** Used by ESPHome framework. */
  void update() override;
  /** Used by ESPHome framework. */
  float get_setup_priority() const override;

  /** Used by ESPHome framework. */
  void set_internal_temperature_sensor(sensor::Sensor *sensor) { internal_temperature_sensor_ = sensor; }
  /** Used by ESPHome framework. */
  void set_external_temperature_sensor(sensor::Sensor *sensor) { external_temperature_sensor_ = sensor; }
  /** Used by ESPHome framework. */
  void set_speed_sensor(sensor::Sensor *sensor) { speed_sensor_ = sensor; }
  /** Used by ESPHome framework. */
  void set_duty_cycle_sensor(sensor::Sensor *sensor) { duty_cycle_sensor_ = sensor; }

 protected:
  Emc2101Component *parent_;
  sensor::Sensor *internal_temperature_sensor_{nullptr};
  sensor::Sensor *external_temperature_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *duty_cycle_sensor_{nullptr};
};

}  // namespace emc2101
}  // namespace esphome
