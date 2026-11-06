#include <gtest/gtest.h>
#include <cmath>

#include "esphome/components/absolute_humidity/absolute_humidity.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::absolute_humidity::testing {

// Expected g/m³ at 50 % RH, from the Buck equations and vapor_density() in double precision
// (es over ice at -10 °C is 0.25995 kPa, over water at 20 °C 2.33834 kPa)
struct Fixture {
  sensor::Sensor temperature;
  sensor::Sensor humidity;
  AbsoluteHumidityComponent component;

  float run(float temperature_c, float relative_humidity) {
    this->component.set_temperature_sensor(&this->temperature);
    this->component.set_humidity_sensor(&this->humidity);
    this->component.set_equation(BUCK);
    this->component.setup();
    this->temperature.publish_state(temperature_c);
    this->humidity.publish_state(relative_humidity);
    this->component.loop();
    return this->component.get_state();
  }
};

TEST(AbsoluteHumidityBuck, UsesIceConstantsBelowZero) {
  Fixture f;
  EXPECT_NEAR(f.run(-10.0f, 50.0f), 1.07018f, 0.0005f);
}

TEST(AbsoluteHumidityBuck, ColderIceValue) {
  Fixture f;
  EXPECT_NEAR(f.run(-30.0f, 50.0f), 0.16942f, 0.0005f);
}

TEST(AbsoluteHumidityBuck, UsesWaterConstantsAboveZero) {
  Fixture f;
  EXPECT_NEAR(f.run(20.0f, 50.0f), 8.64161f, 0.001f);
}

}  // namespace esphome::absolute_humidity::testing
