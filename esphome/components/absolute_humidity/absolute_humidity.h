#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::absolute_humidity {

/// Enum listing all implemented saturation vapor pressure equations.
enum SaturationVaporPressureEquation {
  BUCK,
  TETENS,
  WOBUS,
};

/// This class implements calculation of absolute humidity from temperature and relative humidity.
class AbsoluteHumidityComponent : public sensor::Sensor, public Component {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }
  void set_equation(SaturationVaporPressureEquation equation) { this->equation_ = equation; }

  void setup() override;
  void dump_config() override;
  void loop() override;

 protected:
  /** Buck equation for saturation vapor pressure in kPa.
   *
   * @param temperature_c Air temperature in °C.
   */
  static float es_buck(float temperature_c);
  /** Tetens equation for saturation vapor pressure in kPa.
   *
   * @param temperature_c Air temperature in °C.
   */
  static float es_tetens(float temperature_c);
  /** Wobus equation for saturation vapor pressure in kPa.
   *
   * @param temperature_c Air temperature in °C.
   */
  static float es_wobus(float temperature_c);

  /** Calculate vapor density (absolute humidity) in g/m³.
   *
   * @param es Saturation vapor pressure in kPa.
   * @param hr Relative humidity 0 to 1.
   * @param ta Absolute temperature in K.
   */
  static float vapor_density(float es, float hr, float ta);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  float temperature_{NAN};
  float humidity_{NAN};
  SaturationVaporPressureEquation equation_;
};

}  // namespace esphome::absolute_humidity
