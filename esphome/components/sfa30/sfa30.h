#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome {
namespace sfa30 {

class SFA30Component : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  enum ErrorCode { DEVICE_MARKING_READ_FAILED, MEASUREMENT_INIT_FAILED, UNKNOWN };

 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_formaldehyde_sensor(sensor::Sensor *formaldehyde) { this->formaldehyde_sensor_ = formaldehyde; }
  void set_humidity_sensor(sensor::Sensor *humidity) { this->humidity_sensor_ = humidity; }
  void set_temperature_sensor(sensor::Sensor *temperature) { this->temperature_sensor_ = temperature; }

 protected:
  char device_marking_[32] = {0};

  ErrorCode error_code_{UNKNOWN};

  sensor::Sensor *formaldehyde_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace sfa30
}  // namespace esphome
