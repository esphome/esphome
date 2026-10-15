#pragma once

#include <array>

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

  /// Parses a Home Assistant attribute as an integer from 1 to max, logging anything else
  optional<uint16_t> parse_integer_(const char *name, StringRef value, uint16_t max);
  /// A state or attribute report: answers a pending echo and schedules the apply
  void on_report_(bool changed);
  /// The state a transition or flash ends at, packed, to tell its end from a newer local call
  static uint16_t target_key_(const light::LightColorValues &values);
  /// Schedules apply_imported_() once for the attributes of one update
  void schedule_apply_();
  void apply_imported_();
  /// True when Home Assistant reported exactly what a change would make of it
  bool is_imported_(const Attributes &outgoing) const;
  /// Returns the light to the reported state after a change that did not reach Home Assistant
  void fall_back_();

  const char *entity_id_{nullptr};
  light::LightState *state_{nullptr};
  light::LightTraits traits_{};
  Attributes imported_{};
  uint16_t held_target_{0};  // target_key_() of the transition a report was held for
  uint16_t settled_key_{0};  // target_key_() of the last state written outside a transition
  bool apply_scheduled_ : 1 {false};
  bool initial_write_ : 1 {true};    // the light core's own first write is not a local change
  bool echo_pending_ : 1 {false};    // a sent change waits for Home Assistant to report it back
  bool report_held_ : 1 {false};     // a report arrived during a transition or flash; applied when it ends
  bool report_changed_ : 1 {false};  // the reports since the last apply changed an attribute
};

}  // namespace esphome::homeassistant
