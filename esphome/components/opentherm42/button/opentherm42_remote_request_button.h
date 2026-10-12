#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.3 Class 3, ID 4 HB: one Request-Code, fixed per button instance -- both hub and code are
// required and never change after construction, so they're constructor parameters (see CLAUDE.md's
// "Constructor parameters vs setters" rule) rather than setters.
class OpenTherm42RemoteRequestButton : public button::Button, public Component {
 public:
  OpenTherm42RemoteRequestButton(OpenTherm42Hub *hub, uint8_t code) : hub_(hub), code_(code) {}

 protected:
  void press_action() override { this->hub_->send_remote_request(this->code_); }

  OpenTherm42Hub *hub_;
  uint8_t code_;
};

}  // namespace esphome::opentherm42
