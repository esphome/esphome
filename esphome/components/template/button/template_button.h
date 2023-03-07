#pragma once

#include "esphome/components/button/button.h"

namespace esphome {
namespace template_ {

class TemplateButton : public button::Button {
 public:
  // Implements the abstract `press_action` but the `on_press` trigger already handles the press.
  void press_action() override{};
};

}  // namespace template_
}  // namespace esphome
