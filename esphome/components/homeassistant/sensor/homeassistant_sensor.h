#pragma once

#include "esphome/components/sensor/sensor.h"

namespace esphome::homeassistant {

class HomeassistantSensor final : public sensor::Sensor {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }
  void set_attribute(const char *attribute) { this->attribute_ = attribute; }
  void subscribe();
  void dump_config();

 protected:
  const char *entity_id_{nullptr};
  const char *attribute_{nullptr};
};

}  // namespace esphome::homeassistant
