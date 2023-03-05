#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace absolute_humidity {

enum SaturatedVaporPressureEquation {
  BUCK,
  TETENS,
  WOBUS,
};

class AbsoluteHumidityComponent : public sensor::Sensor, public Component {
 public:
  AbsoluteHumidityComponent() = default;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_equation(SaturatedVaporPressureEquation equation) { equation_ = equation; }

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

  // Saturation vapor pressure equations
  static double es_buck(float temperature_c);
  static double es_tetens(float temperature_c);
  static double es_wobus(float temperature_c);

  // Absolute humidity
  static float vapor_density(double es, float hr, float ta);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  bool next_update_{false};

  float temperature_{NAN};
  float humidity_{NAN};
  SaturatedVaporPressureEquation equation_;
};

}  // namespace absolute_humidity
}  // namespace esphome
