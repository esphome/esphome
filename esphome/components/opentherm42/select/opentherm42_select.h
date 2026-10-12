#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// A select whose value is written to the boiler every essential rotation, never read back (the
// data-id it backs is spec Msg-type "R -": only READ-DATA/READ-ACK are used on the wire, and the
// content this select controls is the master's own -- there is nothing to confirm or reject).
// control()/setup() push the commanded index to the hub via set_solar_storage_solar_mode_write_value()
// rather than relying on this entity's own ->active_index() -- that accessor returns nullopt once
// invalidated, which would otherwise both stop the hub from recovering this select out of Unknown
// on a later success AND silently send the wrong value on the wire in the meantime (see hub.h's
// set_solar_storage_solar_mode_write_value() and hub.cpp's SOLAR_STORAGE_STATUS handling). hub is
// required and never changes after construction (see CLAUDE.md's "Constructor parameters vs
// setters" rule).
class OpenTherm42Select : public select::Select, public Component {
 public:
  explicit OpenTherm42Select(OpenTherm42Hub *hub) : hub_(hub) {}

  void set_initial_option(const std::string &initial_option) { this->initial_option_ = initial_option; }

 protected:
  void setup() override;
  void control(size_t index) override;
  void dump_config() override;

  OpenTherm42Hub *hub_;
  std::string initial_option_;
  ESPPreferenceObject pref_;
};

}  // namespace esphome::opentherm42
