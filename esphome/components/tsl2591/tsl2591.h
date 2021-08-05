#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "Adafruit_TSL2591.h"

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
  TSL2591_GAIN_MULTIPLIER_LOW  = TSL2591_GAIN_LOW,
  TSL2591_GAIN_MULTIPLIER_MED  = TSL2591_GAIN_MED,
  TSL2591_GAIN_MULTIPLIER_HIGH = TSL2591_GAIN_HIGH,
  TSL2591_GAIN_MULTIPLIER_MAX  = TSL2591_GAIN_MAX,
};

/** Enum listing sensor channels.
 *
 * These are mere clones of the values from the Adafruit library.
 * They identify the type of light to measure.
 */
enum TSL2591SensorChannel {
  TSL2591_SENSOR_CHANNEL_VISIBLE       = TSL2591_VISIBLE,
  TSL2591_SENSOR_CHANNEL_INFRARED      = TSL2591_INFRARED,
  TSL2591_SENSOR_CHANNEL_FULL_SPECTRUM = TSL2591_FULLSPECTRUM,
};

/// This class includes support for the TSL2591 i2c ambient light sensor.
class TSL2591Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_infrared_sensor(sensor::Sensor *infrared_sensor);
  void set_visible_sensor(sensor::Sensor *visible_sensor);
  void set_full_spectrum_sensor(sensor::Sensor *full_spectrum_sensor);

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
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_LOW`
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_MED` (default)
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_HIGH`
   *  - `esphome::tsl2591::TSL2591_GAIN_MULTIPLIER_MAX`
   *
   * @param gain The new gain.
   */
  void set_gain(TSL2591Gain gain);

  /** Get the combined illuminance value.
   *
   * This is encoded into a 32bit value. The high 16 bit are the value of the
   * infrared sensor. The low 16 bits are the sum of the combined sensor values.
   */
  uint32_t getCombinedIlluminance();

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
  uint16_t getIlluminance(TSL2591SensorChannel channel);

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
  uint16_t getIlluminance(TSL2591SensorChannel channel, uint32_t combined_illuminance);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these. They're for ESPhome integration use.)
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

protected:
  Adafruit_TSL2591 tsl2591_ = Adafruit_TSL2591(2591);
  bool disabled_{false};
  sensor::Sensor *infrared_sensor_;
  sensor::Sensor *visible_sensor_;
  sensor::Sensor *full_spectrum_sensor_;
};

}  // namespace tsl2591
}  // namespace esphome
