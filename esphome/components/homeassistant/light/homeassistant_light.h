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
  /// Parses a Home Assistant attribute as an integer from 1 to max, logging a value outside that
  optional<uint16_t> parse_integer_(const char *name, StringRef value, uint16_t max);
  /// Schedules apply_imported_() once for the attributes of one update
  void schedule_apply_();
  void apply_imported_();

  const char *entity_id_{nullptr};
  light::LightState *state_{nullptr};
  light::LightTraits traits_{};
  Attributes imported_{};
  bool apply_scheduled_{false};
  bool import_waiting_{false};  // a report waits for a local change to be written first
};

}  // namespace esphome::homeassistant
