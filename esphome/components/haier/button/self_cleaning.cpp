#include "self_cleaning.h"

namespace esphome {
namespace haier {

void SelfCleaningButton::press_action() { this->parent_->start_self_cleaning(); }

}  // namespace haier
}  // namespace esphome
