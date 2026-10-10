#include "self_clean_button.h"
#include "esphome/core/log.h"

namespace esphome::haier_tundra {

ESPHOME_LOG_TAG(TAG, "haier_tundra.button.self_clean");

void SelfCleanButton::dump_config() { LOG_BUTTON("", "Self Clean Button", this); }

void SelfCleanButton::press_action() { this->parent_->start_self_clean(); }

}  // namespace esphome::haier_tundra
