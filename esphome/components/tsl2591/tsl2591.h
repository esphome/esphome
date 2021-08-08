#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

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

/// This class includes support for the TSL2591 i2c ambient light sensor.
/// The device has two distinct sensors. One is for visible light and the
/// other is for infrared light. They are reported as separate sensors,
/// and the combined value "full spectrum" is reported as a third sensor
/// as a convenience. It is merely the sum of the first two.
///
/// This is a fairly thin ESPHome compatibility wrapper around the
/// Adafruit TSL2591 library. Adafruit uses the term "luminosity", but
/// ESPHome uses the term "illuminance", as does the TLS2591 datasheet.
class TSL2591Component : public PollingComponent, public i2c::I2CDevice {
 public:
  /** Set the time that sensor values should be accumulated for.
   *
   * Longer means more accurate, but also mean more power consumption.
   * If you change this parameter at runtime, you should ignore the
   * first subsequent sensor readings since the ADC values will not be reliable.
   *
   * Possible values are:
   *
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_100MS` (default)
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_200MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_300MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_400MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_500MS`
   *  - `esphome::tsl2591::TSL2591_INTEGRATION_TIME_600MS`
   *
   * @param integration_time The new integration time.
   */
  void set_integration_time(TSL2591IntegrationTime integration_time);

  /** Set the internal gain of the sensor. Can be useful for low-light conditions
   *
   * When using the API to set the gain, you should first disable the
   * TSL2591 device. Otherwise, the next set of ADC readings will not be reliable.
   *
   * Possible values are:
   *
   *  - `esphome::tsl2591::TSL2591_GAIN_LOW`  (1x)
   *  - `esphome::tsl2591::TSL2591_GAIN_MED`  (25x, default)
   *  - `esphome::tsl2591::TSL2591_GAIN_HIGH` (400x)
   *  - `esphome::tsl2591::TSL2591_GAIN_MAX`  (9500x)
   *
   * @param gain The new gain.
   */
  void set_gain(TSL2591Gain gain);

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
  boolean is_adc_valid();

  /** Powers on the TSL2591 device and enables its sensors.
   *
   * You only need to call this if you have disabled the device.
   * The device starts enabled in ESPHome.
   */
  void enable();
  /** Powers off the TSL2591 device.
   *
   * You can call this from an ESPHome script if you are explicitly
   * controlling TSL2591 power consumption.
   * The device starts enabled in ESPHome.
   */
  void disable();

  // These methods are normally called by ESPHome core at the right time, based
  // on config in YAML.
  void set_full_spectrum_sensor(sensor::Sensor *full_spectrum_sensor);
  void set_infrared_sensor(sensor::Sensor *infrared_sensor);
  void set_visible_sensor(sensor::Sensor *visible_sensor);
  void set_calculated_lux_sensor(sensor::Sensor *calculated_lux_sensor);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these. They're for ESPHome integration use.)
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

 protected:
  const char *name_;  // TODO: extend esphome::Nameable
  sensor::Sensor *full_spectrum_sensor_;
  sensor::Sensor *infrared_sensor_;
  sensor::Sensor *visible_sensor_;
  sensor::Sensor *calculated_lux_sensor_;
  TSL2591IntegrationTime integration_time_;
  TSL2591Gain gain_;
  bool power_save_mode_enabled_;
  void disable_internal_();
};

}  // namespace tsl2591
}  // namespace esphome
