#include "gy39v3.h"
#include "esphome/core/log.h"

namespace esphome::gy39v3 {

const char GY39v3::TAG[] PROGMEM = "gy39v3.sensor";

void GY39v3::setup() {
  uint8_t initialize;  // simply try to read elevation-low
  if (this->read_register(13, &initialize, 1) != i2c::ERROR_OK) {
    this->mark_failed();  // Mark the component as failed if communication fails
    return;
  }
}

// void GY39v3::loop() {}

void GY39v3::update() {
  // read sensor data
  uint8_t data[14];
  read_register(0, data, sizeof(data));

  // calculate measurements
  const float illuminance =
      static_cast<float>((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3] << 0)) / 100.f;
  const float temperature = static_cast<float>((data[4] << 8) | data[5]) / 100.f;
  const float atm_pressure =
      static_cast<float>((data[6] << 24) | (data[7] << 16) | (data[8] << 8) | (data[9] << 0)) / 100.f;
  const float humidity = static_cast<float>((data[10] << 8) | data[11]) / 100.f;
  const float altitude = static_cast<float>((data[12] << 8) | data[13]);
  const float vpd = calculate_vpd(temperature, humidity);

  // publish values
  if (illuminance_sensor_)
    illuminance_sensor_->publish_state(illuminance);
  if (temperature_sensor_)
    temperature_sensor_->publish_state(temperature);
  if (pressure_sensor_)
    pressure_sensor_->publish_state(atm_pressure);
  if (humidity_sensor_)
    humidity_sensor_->publish_state(humidity);
  if (altitude_sensor_)
    altitude_sensor_->publish_state(altitude);
  if (vapor_pressure_deficit_sensor_ && !std::isnan(vpd))
    vapor_pressure_deficit_sensor_->publish_state(vpd);
}

void GY39v3::dump_config() { ESP_LOGCONFIG(TAG, "Empty compound sensor"); }

float GY39v3::calculate_vpd(float t, float rh) {
  if (std::isnan(delta_))
    return delta_;
  else {
    const float T = (t + delta_) * (9.f / 5.f) + 491.67f;
    const float TT = T * T;
    const float A = -1.0440397e4f;
    const float B = -11.29465f;
    const float C = -2.7022355e-2f;
    const float D = 1.289036e-5f;
    const float E = -2.4780581e-9f;
    const float F = 6.5459673f * std::log(T);
    const float ex = A / T + B + C * T + D * TT + E * TT * T + F;
    const float vpsat = exp(ex);

    return vpsat * (1 - rh);
  }
}

}  // namespace esphome::gy39v3
