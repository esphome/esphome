#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/optional.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

class HomeassistantNumber final : public number::Number {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }

  void subscribe();
  void dump_config();

 protected:
  void state_changed_(StringRef state);
  optional<float> parse_attribute_(const char *name, StringRef value);

  void control(float value) override;

  const char *entity_id_{nullptr};
};
}  // namespace esphome::homeassistant
