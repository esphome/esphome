#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.4 Class 4, IDs 20/21/22: forces an immediate Day-of-week/Time, Date and Year resync attempt,
// ahead of the essential rotation's own periodic writes. hub is required and never changes after
// construction (see CLAUDE.md's "Constructor parameters vs setters" rule).
class OpenTherm42SyncTimeButton : public button::Button, public Component {
 public:
  explicit OpenTherm42SyncTimeButton(OpenTherm42Hub *hub) : hub_(hub) {}

 protected:
  void press_action() override { this->hub_->push_time_sync(); }

  OpenTherm42Hub *hub_;
};

}  // namespace esphome::opentherm42
