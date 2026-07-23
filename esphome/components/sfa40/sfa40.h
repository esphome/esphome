#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::sfa40 {

// SFA40 Datasheet: https://sensirion.com/media/documents/5B06EDD9/69F84BD8/Sensirion_Datasheet_SFA40.pdf

class SFA40Component final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  enum ProtocolVersion : uint8_t { PRODUCTION = 0, PROTOTYPE = 1, };
  enum ErrorCode : uint8_t { PROTOCOL_DETECTION_FAILED, MEASUREMENT_INIT_FAILED, UNKNOWN };

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_formaldehyde_sensor(sensor::Sensor *formaldehyde) { this->formaldehyde_sensor_ = formaldehyde; }
  void set_temperature_sensor(sensor::Sensor *temperature) { this->temperature_sensor_ = temperature; }
  void set_humidity_sensor(sensor::Sensor *humidity) { this->humidity_sensor_ = humidity; }
  void set_wait_for_ready(bool wait_for_ready) { this->wait_for_ready_ = wait_for_ready; }

 protected:
  bool detect_protocol_();

  ProtocolVersion protocol_version_{PRODUCTION};
  char device_marking_[32] = {0};
  bool initialized_{false};
  ErrorCode error_code_{UNKNOWN};
  bool wait_for_ready_{false};

  sensor::Sensor *formaldehyde_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
};

}  // namespace esphome::sfa40