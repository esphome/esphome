#pragma once

#include <array>

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

/// A light in Home Assistant, driven from here: its state and attributes are imported over the
/// native API, and every change made on this side is sent back as a light.turn_on or
/// light.turn_off action.
class HomeassistantLight final : public light::LightOutput, public Component {
 public:
  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }
  /// How long a sent change may take to come back as a report before the light returns to the reported state
  void set_report_timeout(uint8_t seconds) { this->report_timeout_s_ = seconds; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  light::LightTraits get_traits() override { return this->traits_; }
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void update_state(light::LightState *state) override;
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
  /// A state or attribute report: answers a pending echo when it changed something, and schedules the apply
  void on_report_(bool changed);
  /// Takes over a reported attribute, reporting whether it changed
  template<typename T> void import_(T &field, const T &value) {
    this->on_report_(field != value);
    field = value;
  }
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
  bool apply_scheduled_ : 1 {false};
  bool initial_write_ : 1 {true};  // the light core's own first write is not a local change
  bool initial_seen_ : 1 {false};  // the light core's setup call has set the initial state
  bool echo_pending_ : 1 {false};  // a sent change waits for Home Assistant to report it back
  bool report_held_ : 1 {false};   // a report arrived during a transition or flash; applied when it ends
  bool transitioned_ : 1 {false};  // a transition or flash ran since the last write sent
  uint8_t report_timeout_s_{10};
  uint8_t report_call_{0};  // the light's call count when the last changed report was read
};

}  // namespace esphome::homeassistant
