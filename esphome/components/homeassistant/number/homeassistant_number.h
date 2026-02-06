#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"

namespace esphome {
namespace homeassistant {

class HomeassistantNumber : public number::Number, public Component {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void state_changed_(StringRef state);
  void min_retrieved_(StringRef min);
  void max_retrieved_(StringRef max);
  void step_retrieved_(StringRef step);

  void control(float value) override;

  const char *entity_id_{nullptr};
};
}  // namespace homeassistant
}  // namespace esphome
