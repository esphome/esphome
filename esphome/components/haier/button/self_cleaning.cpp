#include "self_cleaning.h"

namespace esphome::haier {

void SelfCleaningButton::press_action() { this->parent_->start_self_cleaning(); }

}  // namespace esphome::haier
