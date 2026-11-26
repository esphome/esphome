#pragma once

#ifdef USE_ESP32

#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../ads131m0x.h"

namespace esphome::ads131m0x {

enum ADS131M0xSensorType : uint8_t {
  ADS131M0X_SENSOR_TYPE_RMS = 0,
  ADS131M0X_SENSOR_TYPE_MEAN = 1,
};

class ADS131M0xSensor final : public PollingComponent, public sensor::Sensor, public Parented<ADS131M0x> {
 public:
  void update() override;
  void dump_config() override;

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_type(ADS131M0xSensorType type) { this->type_ = type; }

 protected:
  uint8_t channel_{0};
  ADS131M0xSensorType type_{ADS131M0X_SENSOR_TYPE_RMS};
};

}  // namespace esphome::ads131m0x

#endif  // USE_ESP32
