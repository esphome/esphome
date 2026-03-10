#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace homeassistant {

class HomeassistantSensor : public sensor::Sensor, public Component {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }
  void set_attribute(const char *attribute) { this->attribute_ = attribute; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  const char *entity_id_{nullptr};
  const char *attribute_{nullptr};
};

}  // namespace homeassistant
}  // namespace esphome
