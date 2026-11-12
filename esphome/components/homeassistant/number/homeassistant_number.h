#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

class HomeassistantNumber final : public number::Number, public Component {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void state_changed_(StringRef state);
  /// Parses a min/max/step attribute, logging the result
  optional<float> parse_attribute_(const char *name, StringRef value);

  void control(float value) override;

  const char *entity_id_{nullptr};
};
}  // namespace esphome::homeassistant
