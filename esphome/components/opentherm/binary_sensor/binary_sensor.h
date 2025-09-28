#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/opentherm/hub.h"

namespace esphome {
namespace opentherm {

// A simple true/false sensor
template<typename T> class OpenthermBinarySensor : public binary_sensor::BinarySensor, public MessageProcessor {
  static_assert(std::is_same<typename T::ValueType, bool>::value, "T::ValueType must be bool");

 public:
  void prepare_data_out(OpenthermData &data) const override { data.type = MessageType::READ_DATA; }
  void parse_and_publish(const OpenthermData &data) override { this->publish_state(T::get(data)); }
  const char *get_type_name() const override { return "binary_sensor"; }
};

}  // namespace opentherm
}  // namespace esphome
