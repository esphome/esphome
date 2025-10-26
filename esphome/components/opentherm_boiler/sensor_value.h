#pragma once

#include "boiler.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageType;

template<typename T> class SensorValue : public RequestProcessor {
 public:
  SensorValue(sensor::Sensor *sensor) : sensor_(sensor) {}

  bool handle_request(OpenthermData &data) override {
    if (data.type == MessageType::READ_DATA && this->sensor_->has_state()) {
      T::set(data, this->sensor_->get_state());
      return true;
    }

    return false;
  }

  const char *get_type_name() const override { return "sensor_value"; }

 protected:
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace opentherm_boiler
}  // namespace esphome
