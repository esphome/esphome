#pragma once

#include "../boiler.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageType;

template<typename T> class BoilerBinarySensor : public binary_sensor::BinarySensor, public RequestProcessor {
 public:
  BoilerBinarySensor() = default;

  bool handle_request(OpenthermData &data) override {
    if (data.type == MessageType::WRITE_DATA || (data.type == MessageType::READ_DATA && data.id == MessageId::STATUS)) {
      // STATUS messages are always READ_DATA, but we treat them like WRITE_DATA to update the state
      this->publish_state(T::get(data));
      return true;
    } else if (data.type == MessageType::READ_DATA && this->has_state()) {
      T::set(data, this->get_state());
      return true;
    }
    return false;
  }

  const char *get_type_name() const override { return "binary_sensor"; }
};

}  // namespace opentherm_boiler
}  // namespace esphome
