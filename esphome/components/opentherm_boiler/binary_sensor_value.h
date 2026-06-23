#pragma once

#include "boiler.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageType;

template<typename T> class BinarySensorValue : public RequestProcessor {
 public:
  BinarySensorValue(binary_sensor::BinarySensor *binary_sensor) : binary_sensor_(binary_sensor) {}

  bool handle_request(OpenthermData &data) override {
    if (data.type == MessageType::READ_DATA && this->binary_sensor_->has_state()) {
      T::set(data, this->binary_sensor_->get_state());
      return true;
    }

    return false;
  }

  const char *get_type_name() const override { return "binary_sensor_value"; }

 protected:
  binary_sensor::BinarySensor *binary_sensor_{nullptr};
};

}  // namespace opentherm_boiler
}  // namespace esphome
