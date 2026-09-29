#include "steri_cleaning.h"

namespace esphome::haier {

void SteriCleaningButton::press_action() { this->parent_->start_steri_cleaning(); }

}  // namespace esphome::haier
