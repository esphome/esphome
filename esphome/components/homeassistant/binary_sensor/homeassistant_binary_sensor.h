#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace homeassistant {

class HomeassistantBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  HomeassistantBinarySensor(const std::string &name, const std::string &entity_id);
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  std::string entity_id_;
};

}  // namespace homeassistant
}  // namespace esphome
