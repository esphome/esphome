#pragma once

#include "esphome/components/button/button.h"

namespace esphome::template_ {

class TemplateButton final : public button::Button {
 public:
  // Implements the abstract `press_action` but the `on_press` trigger already handles the press.
  void press_action() override{};
};

}  // namespace esphome::template_
