#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace absolute_humidity {

/// Enum listing all implemented saturation vapor pressure equations.
enum SaturationVaporPressureEquation {
  BUCK,
  TETENS,
  WOBUS,
};

/// This class implements calculation of absolute humidity from temperature and relative humidity.
class AbsoluteHumidityComponent : public sensor::Sensor, public Component {
 public:
  AbsoluteHumidityComponent() = default;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }
  void set_equation(SaturationVaporPressureEquation equation) { this->equation_ = equation; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

 protected:
  void temperature_callback_(float state) {
    this->next_update_ = true;
    this->temperature_ = state;
  }
  void humidity_callback_(float state) {
    this->next_update_ = true;
    this->humidity_ = state;
  }

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
   * @param heater_duration The duration in ms that the heater should turn on for when measuring.
   */
  static float vapor_density(float es, float hr, float ta);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  bool next_update_{false};

  float temperature_{NAN};
  float humidity_{NAN};
  SaturationVaporPressureEquation equation_;
};

}  // namespace absolute_humidity
}  // namespace esphome
