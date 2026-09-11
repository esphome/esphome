#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.8.3 Class 8, ID 99 HB bit 4: Manual DHW push2 -- rises the DHW temperature once to Comfort
// level and returns to the previous Operating Mode. hub is required and never changes after
// construction (see CLAUDE.md's "Constructor parameters vs setters" rule).
class OpenTherm42ManualDhwPush2Button : public button::Button, public Component {
 public:
  explicit OpenTherm42ManualDhwPush2Button(OpenTherm42Hub *hub) : hub_(hub) {}

 protected:
  void press_action() override { this->hub_->push_manual_dhw_push2(); }

  OpenTherm42Hub *hub_;
};

}  // namespace esphome::opentherm42
