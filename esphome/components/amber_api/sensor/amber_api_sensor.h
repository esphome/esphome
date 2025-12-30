#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../amber_api.h"

namespace esphome {
namespace amber_api {

enum AmberApiSensorType : uint8_t {
  GENERAL = 0,
  GENERAL_FORECAST = 1,
  FEEDIN = 2,
  FEEDIN_FORECAST = 3,
};

class AmberApiSensor : public sensor::Sensor, public Component, public AmberApiListener {
 public:
  void set_sensor_type(AmberApiSensorType type) { this->type_ = type; }

  void dump_config() override;
  void on_amber_api_update(const AmberApiData &data) override;

 protected:
  AmberApiSensorType type_{GENERAL};
};

}  // namespace amber_api
}  // namespace esphome
