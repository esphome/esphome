#include "query_button.h"

namespace esphome::ld2410 {

void QueryButton::press_action() { this->parent_->read_all_info(); }

}  // namespace esphome::ld2410
