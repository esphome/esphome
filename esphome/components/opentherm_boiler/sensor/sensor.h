#pragma once

#include "../boiler.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageType;

template<typename T> class BoilerSensor : public sensor::Sensor, public RequestProcessor {
 public:
  BoilerSensor() = default;

  bool handle_request(OpenthermData &data) override {
    if (data.type == MessageType::WRITE_DATA) {
      this->publish_state(T::get(data));
      return true;
    } else if (data.type == MessageType::READ_DATA && this->has_state()) {
      T::set(data, this->get_state());
      return true;
    }
    return false;
  }

  const char *get_type_name() const override { return "sensor"; }
};

}  // namespace opentherm_boiler
}  // namespace esphome
