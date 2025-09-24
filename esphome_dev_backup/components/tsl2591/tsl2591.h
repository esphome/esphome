#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome {
namespace tsl2591 {

/** Enum listing all conversion/integration time settings for the TSL2591.
 *
 * Specific values of the enum constants are register values taken from the TSL2591 datasheet.
 * Longer times mean more accurate results, but will take more energy/more time.
 */
enum TSL2591IntegrationTime {
  TSL2591_INTEGRATION_TIME_100MS = 0b000,
  TSL2591_INTEGRATION_TIME_200MS = 0b001,
  TSL2591_INTEGRATION_TIME_300MS = 0b010,
  TSL2591_INTEGRATION_TIME_400MS = 0b011,
  TSL2591_INTEGRATION_TIME_500MS = 0b100,
  TSL2591_INTEGRATION_TIME_600MS = 0b101,
};

/** Enum listing all gain settings for the TSL2591 component.
 *
 * Enum constants are used by the component to allow auto gain, not directly to registers
 * Higher values are better for low light situations, but can increase noise.
 */
enum TSL2591ComponentGain {
  TSL2591_CGAIN_LOW,   // 1x
  TSL2591_CGAIN_MED,   // 25x
  TSL2591_CGAIN_HIGH,  // 400x
  TSL2591_CGAIN_MAX,   // 9500x
  TSL2591_CGAIN_AUTO
};

/** Enum listing all gain settings for the TSL2591.
 *
 * Specific values of the enum constants are register values taken from the TSL2591 datasheet.
 * Higher values are better for low light situations, but can increase noise.
 */
enum TSL2591Gain {
  TSL2591_GAIN_LOW = 0b00 << 4,   // 1x
  TSL2591_GAIN_MED = 0b01 << 4,   // 25x
  TSL2591_GAIN_HIGH = 0b10 << 4,  // 400x
  TSL2591_GAIN_MAX = 0b11 << 4,   // 9500x
};

/** Enum listing sensor channels.
 *
 * They identify the type of light to report.
 */
enum TSL2591SensorChannel {
  TSL2591_SENSOR_CHANNEL_VISIBLE,
  TSL2591_SENSOR_CHANNEL_INFRARED,
  TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM,
};

/// This class includes support for the TSL2591 i2c ambient light
/// sensor.  The device has two distinct sensors. One is for visible
/// light plus infrared light, and the other is for infrared
/// light. They are reported as separate sensors, and the difference
/// between the values is reported as a third sensor as a convenience
/// for visible light only.
class TSL2591Component : public PollingComponent, public i2c::I2CDevice {
 public:
  /** Set device integration time and gain.
   *
   * These are set as a single I2C transaction, so you must supply values
   * for both.
   *
   * Longer integration times provides more accurate values, but also
   * means more power consumption. Higher gain values are useful for
   * lower light intensities but are also subject to more noise. The
   * device might use a slightly different gain multiplier than those
   * indicated; see the datasheet for details.
   *
   * Possible values for integration_time (from enum
   * TSL2591IntegrationTime) are:
   *
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_100MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_200MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_300MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_400MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_500MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_600MS`
   *
   * Possible values for gain (from enum TSL2591Gain) are:
   *
   *  - `esphome::tsl2591::TSL2591_GAIN_LOW`  (1x)
   *  - `esphome::tsl2591::TSL2591_GAIN_MED`  (25x)
   *  - `esphome::tsl2591::TSL2591_GAIN_HIGH` (400x)
   *  - `esphome::tsl2591::TSL2591_GAIN_MAX`  (9500x)
   *
   * @param integration_time The new integration time.
   * @param gain The new gain.
   */
  void set_integration_time_and_gain(TSL2591IntegrationTime integration_time, TSL2591Gain gain);

  /** Should the device be powered down between readings?
   *
   * The disadvantage of powering down the device between readings
   * is that you have to wait for the ADC to go through an
   * integration cycle before a reliable reading is available.
   * This happens during ESPHome's update loop, so waiting slows
   * down the entire ESP device. You should only enable this if
   * you need to minimize power consumption and you can tolerate
   * that delay. Otherwise, keep the default of disabling
   * power save mode.
   *
   * @param enable Enable or disable power save mode.
   */
  void set_power_save_mode(bool enable);

  /** Sets the name for this instance of the device.
   *
   * @param name The user-friendly name.
   */
  void set_name(const char *name);

  /** Sets the device and glass attenuation factors.
   *
   * The lux equation, used to calculate the lux from the ADC readings,
   * involves a scaling coefficient that is the product of a device
   * factor (specific to the type of device being used) and a glass
   * attenuation factor (specific to whatever glass or plastic cover
   * is installed in front of the light sensors.
   *
   * AMS does not publish the device factor for the TSL2591. In the
   * datasheet for the earlier TSL2571 and in application notes, they
   * use the value 53, so we use that as the default.
   *
   * The glass attenuation factor depends on factors external to the
   * TSL2591 and is best obtained through experimental measurements.
   * The Adafruit TSL2591 library use a value of ~7.7, which we use as
   * a default. Waveshare uses a value of ~14.4. Presumably, those
   * factors are appropriate to the breakout boards from those vendors,
   * but we have not verified that.
   *
   * @param device_factor The device factor.
   * @param glass_attenuation_factor The glass attenuation factor.
   */
  void set_device_and_glass_attenuation_factors(float device_factor, float glass_attenuation_factor);

  /** Calculates and returns a lux value based on the ADC readings.
   *
   * @param full_spectrum The ADC reading for the full spectrum sensor.
   * @param infrared The ADC reading for the infrared sensor.
   */
  float get_calculated_lux(uint16_t full_spectrum, uint16_t infrared);

  /** Get the combined illuminance value.
   *
   * This is encoded into a 32 bit value. The high 16 bits are the value of the
   * infrared sensor. The low 16 bits are the sum of the combined sensor values.
   *
   * If power saving mode is enabled, there can be a delay (up to the value of the integration
   * time) while waiting for the device ADCs to signal that values are valid.
   */
  uint32_t get_combined_illuminance();

  /** Get an individual sensor channel reading.
   *
   * This gets an individual light sensor reading. Since it goes through
   * the entire component read cycle to get one value, it's not optimal if
   * you want to get all possible channel values. If you want that, first
   * call `get_combined_illuminance()` and pass that value to the companion
   * method with a different signature.
   *
   * If power saving mode is enabled, there can be a delay (up to the value of the integration
   * time) while waiting for the device ADCs to signal that values are valid.
   *
   * @param channel The sensor channel of interest.
   */
  uint16_t get_illuminance(TSL2591SensorChannel channel);

  /** Get an individual sensor channel reading from combined illuminance.
   *
   * This gets an individual light sensor reading from a combined illuminance
   * value, which you would obtain from calling `getCombinedIlluminance()`.
   * This method does not communicate with the sensor at all. It's strictly
   * local calculations, so it is efficient if you call it multiple times.
   *
   * @param channel The sensor channel of interest.
   * @param combined_illuminance The previously obtained combined illuminance value.
   */
  uint16_t get_illuminance(TSL2591SensorChannel channel, uint32_t combined_illuminance);

  /** Are the device ADC values valid?
   *
   * Useful for scripting. This should be checked before calling update().
   * It asks the TSL2591 if the ADC has completed an integration cycle
   * and has reliable values in the device registers. If you call update()
   * before the ADC values are valid, you may cause a general delay in
   * the ESPHome update loop.
   *
   * It should take no more than the configured integration time for
   * the ADC values to become valid after the TSL2591 device is enabled.
   */
  bool is_adc_valid();

  /** Powers on the TSL2591 device and enables its sensors.
   *
   * You only need to call this if you have disabled the device.
   * The device starts enabled in ESPHome unless power save mode is enabled.
   */
  void enable();
  /** Powers off the TSL2591 device.
   *
   * You can call this from an ESPHome script if you are explicitly
   * controlling TSL2591 power consumption.
   * The device starts enabled in ESPHome unless power save mode is enabled.
   */
  void disable();

  /** Updates the gain setting based on the most recent full spectrum reading
   *
   * This gets called on update and tries to keep the ADC readings in the middle of the range
   */
  void automatic_gain_update(uint16_t full_spectrum);

  /** Reads the actual gain used
   *
   * Useful for exposing the real gain used when configured in "auto" gain mode
   */
  float get_actual_gain();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these. They're for ESPHome integration use.)
  /** Used by ESPHome framework. */
  void set_full_spectrum_sensor(sensor::Sensor *full_spectrum_sensor);
  /** Used by ESPHome framework. */
  void set_actual_gain_sensor(sensor::Sensor *actual_gain_sensor);
  /** Used by ESPHome framework. */
  void set_infrared_sensor(sensor::Sensor *infrared_sensor);
  /** Used by ESPHome framework. */
  void set_visible_sensor(sensor::Sensor *visible_sensor);
  /** Used by ESPHome framework. */
  void set_calculated_lux_sensor(sensor::Sensor *calculated_lux_sensor);
  /** Used by ESPHome framework. Does NOT actually set the value on the device. */
  void set_integration_time(TSL2591IntegrationTime integration_time);
  /** Used by ESPHome framework. Does NOT actually set the value on the device. */
  void set_gain(TSL2591ComponentGain gain);
  /** Used by ESPHome framework. */
  void setup() override;
  /** Used by ESPHome framework. */
  void dump_config() override;
  /** Used by ESPHome framework. */
  void update() override;
  /** Used by ESPHome framework. */
  float get_setup_priority() const override;

 protected:
  const char *name_;
  sensor::Sensor *full_spectrum_sensor_{nullptr};
  sensor::Sensor *infrared_sensor_{nullptr};
  sensor::Sensor *visible_sensor_{nullptr};
  sensor::Sensor *calculated_lux_sensor_{nullptr};
  sensor::Sensor *actual_gain_sensor_{nullptr};
  TSL2591IntegrationTime integration_time_;
  TSL2591ComponentGain component_gain_;
  TSL2591Gain gain_;
  bool power_save_mode_enabled_;
  float device_factor_;
  float glass_attenuation_factor_;
  uint64_t interval_start_;
  uint64_t interval_timeout_;
  void disable_if_power_saving_();
  void process_update_();
  void interval_function_for_update_();
};

}  // namespace tsl2591
}  // namespace esphome
