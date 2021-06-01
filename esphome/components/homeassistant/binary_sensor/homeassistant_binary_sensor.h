#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace homeassistant {

class HomeassistantBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }
  void set_attribute(const std::string &attribute) { attribute_ = attribute; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  std::string entity_id_;
  optional<std::string> attribute_;
  bool initial_{true};
};

}  // namespace homeassistant
}  // namespace esphome
