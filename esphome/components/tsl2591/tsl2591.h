#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "Adafruit_TSL2591.h"  // NOLINT

namespace esphome {
namespace tsl2591 {

/** Enum listing all conversion/integration time settings for the TSL2591.
 *
 * These are mere clones of the values from the Adafruit library.
 * Higher values mean more accurate results, but will take more energy/more time.
 */
enum TSL2591IntegrationTime {
  TSL2591_INTEGRATION_TIME_100MS = TSL2591_INTEGRATIONTIME_100MS,
  TSL2591_INTEGRATION_TIME_200MS = TSL2591_INTEGRATIONTIME_200MS,
  TSL2591_INTEGRATION_TIME_300MS = TSL2591_INTEGRATIONTIME_300MS,
  TSL2591_INTEGRATION_TIME_400MS = TSL2591_INTEGRATIONTIME_400MS,
  TSL2591_INTEGRATION_TIME_500MS = TSL2591_INTEGRATIONTIME_500MS,
  TSL2591_INTEGRATION_TIME_600MS = TSL2591_INTEGRATIONTIME_600MS,
};

/** Enum listing all gain settings for the TSL2591.
 *
 * These are mere clones of the values from the Adafruit library.
 * Higher values are better for low light situations, but can increase noise.
 */
enum TSL2591Gain {
  TSL2591_GAIN_MULTIPLIER_LOW = TSL2591_GAIN_LOW,    // 1x
  TSL2591_GAIN_MULTIPLIER_MED = TSL2591_GAIN_MED,    // 25x
  TSL2591_GAIN_MULTIPLIER_HIGH = TSL2591_GAIN_HIGH,  // 480x
  TSL2591_GAIN_MULTIPLIER_MAX = TSL2591_GAIN_MAX,    // 9876x
};

/** Enum listing sensor channels.
 *
 * These are mere clones of the values from the Adafruit library.
 * They identify the type of light to measure.
 */
enum TSL2591SensorChannel {
  TSL2591_SENSOR_CHANNEL_VISIBLE = TSL2591_VISIBLE,
  TSL2591_SENSOR_CHANNEL_INFRARED = TSL2591_INFRARED,
  TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM = TSL2591_FULLSPECTRUM,
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
   * Possible values are:
   *
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_LOW`  (1x)
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_MED`  (25x, default)
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_HIGH` (480x)
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_MAX`  (9876x)
   *
   * @param gain The new gain.
   */
  void set_gain(TSL2591Gain gain);

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
   * call `getCombinedIlluminance()` and pass that value to the companion
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
  Adafruit_TSL2591 tsl2591_ = Adafruit_TSL2591(2591);
  sensor::Sensor *full_spectrum_sensor_;
  sensor::Sensor *infrared_sensor_;
  sensor::Sensor *visible_sensor_;
  sensor::Sensor *calculated_lux_sensor_;
};

}  // namespace tsl2591
}  // namespace esphome
