#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace homeassistant {

class HomeassistantSensor : public sensor::Sensor, public Component {
 public:
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  std::string entity_id_;
};

}  // namespace homeassistant
}  // namespace esphome
