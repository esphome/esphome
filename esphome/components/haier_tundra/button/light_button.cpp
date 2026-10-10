#include "light_button.h"
#include "esphome/core/log.h"

namespace esphome::haier_tundra {

ESPHOME_LOG_TAG(TAG, "haier_tundra.button.light");

void LightButton::dump_config() { LOG_BUTTON("", "Light Toggle Button", this); }

void LightButton::press_action() {
  this->parent_->toggle_light();
}

}  // namespace esphome::haier_tundra
