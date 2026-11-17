#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome::homeassistant {

class HomeassistantButton final : public button::Button, public Component {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }

  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void press_action() override;

  const char *entity_id_{nullptr};
};

}  // namespace esphome::homeassistant
