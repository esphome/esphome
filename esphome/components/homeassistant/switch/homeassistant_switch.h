#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace homeassistant {

class HomeassistantSwitch : public switch_::Switch, public Component {
 public:
  void set_entity_id(const std::string &entity_id) { this->entity_id_ = entity_id; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void write_state(bool state) override;
  std::string entity_id_;
};

}  // namespace homeassistant
}  // namespace esphome
