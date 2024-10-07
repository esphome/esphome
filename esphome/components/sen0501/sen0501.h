#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/DFRobot/DFRobot_EnvironmentalSensor

namespace esphome {
namespace sen0501 {

class Sen0501Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { this->humidity_ = humidity; }
  void set_uv_intensity(sensor::Sensor *uv_intensity) { this->uv_intensity_ = uv_intensity; }
  void set_luminous_intensity(sensor::Sensor *luminous_intensity) { this->luminous_intensity_ = luminous_intensity; }
  void set_atmospheric_pressure(sensor::Sensor *atmospheric_pressure) {
    this->atmospheric_pressure_ = atmospheric_pressure;
  }
  void set_elevation(sensor::Sensor *elevation) { this->elevation_ = elevation; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  void read_temperature_();
  void read_humidity_();
  void read_uv_intensity_();
  void read_luminous_intensity_();
  void read_atmospheric_pressure_();

  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *uv_intensity_{nullptr};
  sensor::Sensor *luminous_intensity_{nullptr};
  sensor::Sensor *atmospheric_pressure_{nullptr};
  sensor::Sensor *elevation_{nullptr};

  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_DEVICE_ID,
    WRONG_VENDOR_ID,
  } error_code_{NONE};
};

}  // namespace sen0501
}  // namespace esphome
