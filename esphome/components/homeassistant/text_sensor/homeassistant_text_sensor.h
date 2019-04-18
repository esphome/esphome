#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace homeassistant {

class HomeassistantTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }
  void dump_config() override;
  void setup() override;

 protected:
  std::string entity_id_;
};

}  // namespace homeassistant
}  // namespace esphome
