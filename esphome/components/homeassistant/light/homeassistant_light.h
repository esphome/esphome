#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

/// A light in Home Assistant, driven from here: its state and attributes are imported over the
/// native API, and every change made on this side is sent back as a light.turn_on or
/// light.turn_off action.
class HomeassistantLight final : public light::LightOutput, public Component {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  light::LightTraits get_traits() override { return this->traits_; }
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;

 protected:
  /// The attributes of one Home Assistant update, applied together on the next loop pass
  struct Pending {
    optional<bool> state;
    optional<light::ColorMode> color_mode;
    optional<float> brightness;
    optional<float> color_temp;
    optional<std::array<uint8_t, 3>> rgb;
  };

  /// Parses a Home Assistant attribute as a number, logging the attribute name when it is not one
  optional<float> parse_attribute_(const char *name, StringRef value);
  /// Records part of an update and schedules apply_pending_() once
  template<typename T> void import_(optional<T> &field, T value);
  void apply_pending_();

  const char *entity_id_{nullptr};
  light::LightState *state_{nullptr};
  light::LightTraits traits_{};
  light::LightColorValues imported_{};  // the values Home Assistant reported last
  Pending pending_{};
  bool apply_scheduled_{false};
};

}  // namespace esphome::homeassistant
