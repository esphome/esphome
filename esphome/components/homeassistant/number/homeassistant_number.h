#pragma once

#include <map>
#include <string>

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

namespace esphome {
namespace homeassistant {

class HomeassistantNumber : public number::Number, public Component {
 public:
  void set_entity_id(const std::string &entity_id) { this->entity_id_ = entity_id; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void state_changed_(const std::string &state);
  void min_retrieved_(const std::string &min);
  void max_retrieved_(const std::string &max);
  void step_retrieved_(const std::string &step);

  void control(float value) override;

  std::string entity_id_;
};
}  // namespace homeassistant
}  // namespace esphome
