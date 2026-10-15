#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

/// A light in Home Assistant, driven from here: its state and attributes are imported over the
/// native API, and every change made on this side is sent back as a light.turn_on or
/// light.turn_off action.
class HomeassistantLight final : public light::LightOutput, public light::LightRemoteValuesListener, public Component {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  light::LightTraits get_traits() override { return this->traits_; }
  void setup_state(light::LightState *state) override {
    this->state_ = state;
    state->add_remote_values_listener(this);
  }
  void write_state(light::LightState *state) override;
  void on_light_remote_values_update() override { this->remote_values_version_++; }

 protected:
  /// What Home Assistant reported last, in its own units. Each attribute arrives as its own
  /// message and stays until the next report of it, so an update may change any subset; zero,
  /// ColorMode::UNKNOWN and has_rgb say an attribute was never reported.
  struct Attributes {
    uint16_t color_temp_kelvin{0};
    uint8_t brightness{0};
    std::array<uint8_t, 3> rgb{};
    light::ColorMode color_mode{light::ColorMode::UNKNOWN};
    bool has_rgb{false};
    optional<bool> state;
  };

  /// Parses a Home Assistant attribute as a number, logging the attribute name when it is not one
  optional<float> parse_attribute_(const char *name, StringRef value);
  /// Parses a color temperature in Kelvin, logging a value that is not usable
  optional<uint16_t> parse_kelvin_(const char *name, StringRef value);
  /// Schedules apply_imported_() once for the attributes of one update
  void schedule_apply_();
  void apply_imported_();

  const char *entity_id_{nullptr};
  light::LightState *state_{nullptr};
  light::LightTraits traits_{};
  Attributes imported_{};
  // Every published change counts, so write_state() can tell the one apply_imported_() made
  // from a local change, even one made from an automation while that import was applied. The
  // initial state of LightState::setup() is version 1 and is not sent either.
  uint32_t remote_values_version_{0};
  uint32_t import_version_{1};
  bool apply_scheduled_{false};
};

}  // namespace esphome::homeassistant
