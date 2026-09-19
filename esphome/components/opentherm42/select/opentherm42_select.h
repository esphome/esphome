#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::opentherm42 {

// A select whose value is written to the boiler every essential rotation, never read back (the
// data-id it backs is spec Msg-type "R -": only READ-DATA/READ-ACK are used on the wire, and the
// content this select controls is the master's own -- there is nothing to confirm or reject).
// Behaves like OpenTherm42Switch: control() publishes optimistically, and the hub resends
// whatever this last published as long as it's configured -- see hub.cpp's build_next_request_().
class OpenTherm42Select : public select::Select, public Component {
 public:
  void set_initial_option(const std::string &initial_option) { this->initial_option_ = initial_option; }

 protected:
  void setup() override;
  void control(size_t index) override;
  void dump_config() override;

  std::string initial_option_;
  ESPPreferenceObject pref_;
};

}  // namespace esphome::opentherm42
