#pragma once

#include "esphome/components/switch/switch.h"

namespace esphome::homeassistant {

class HomeassistantSwitch final : public switch_::Switch {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }
  void subscribe();
  void dump_config();

 protected:
  void write_state(bool state) override;
  const char *entity_id_{nullptr};
};

}  // namespace esphome::homeassistant
