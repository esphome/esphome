#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace homeassistant {

class HomeassistantTextSensor : public text_sensor::TextSensor, public Component {
 public:
  HomeassistantTextSensor(const std::string &name, const std::string &entity_id)
      : TextSensor(name), entity_id_(entity_id) {}
  void dump_config() override;
  void setup() override;

 protected:
  std::string entity_id_;
};

}  // namespace homeassistant
}  // namespace esphome
