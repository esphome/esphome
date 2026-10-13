#include "haier_tundra_button.h"
#include "esphome/core/log.h"

namespace esphome::haier_tundra {

ESPHOME_LOG_TAG(TAG, "haier_tundra.button");

void HaierTundraButton::dump_config() { LOG_BUTTON("", "Haier Tundra Button", this); }

void HaierTundraButton::press_action() {
  if (this->kind_ == ButtonKind::BUTTON_KIND_LIGHT) {
    this->parent_->toggle_light();
  } else {
    this->parent_->start_self_clean();
  }
}

}  // namespace esphome::haier_tundra
