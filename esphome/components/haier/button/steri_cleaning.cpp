#include "steri_cleaning.h"

namespace esphome {
namespace haier {

void SteriCleaningButton::press_action() { this->parent_->start_steri_cleaning(); }

}  // namespace haier
}  // namespace esphome
