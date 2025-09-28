#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/opentherm/hub.h"

namespace esphome {
namespace opentherm {

// A simple true/false sensor
template<typename T> class OpenthermSensor : public sensor::Sensor, public MessageProcessor {
 public:
  void prepare_data_out(OpenthermData &data) const override { data.type = MessageType::READ_DATA; }
  void parse_and_publish(const OpenthermData &data) override { this->publish_state(T::get(data)); }
  const char *get_type_name() const override { return "sensor"; }
};

}  // namespace opentherm
}  // namespace esphome
