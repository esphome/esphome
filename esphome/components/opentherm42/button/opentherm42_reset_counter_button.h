#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.4 Class 4: resets one of the counter/hour data-ids by writing zero to it (optional for the
// boiler to honor). hub and data_id are both required and never change after construction (see
// CLAUDE.md's "Constructor parameters vs setters" rule).
class OpenTherm42ResetCounterButton : public button::Button, public Component {
 public:
  OpenTherm42ResetCounterButton(OpenTherm42Hub *hub, uint8_t data_id) : hub_(hub), data_id_(data_id) {}

 protected:
  void press_action() override { this->hub_->reset_counter(this->data_id_); }

  OpenTherm42Hub *hub_;
  uint8_t data_id_;
};

}  // namespace esphome::opentherm42
