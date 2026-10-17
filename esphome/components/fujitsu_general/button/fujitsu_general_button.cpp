#include "fujitsu_general_button.h"

namespace esphome::fujitsu_general {

void FujitsuGeneralButton::press_action() { this->parent_->transmit_util(this->command_byte_); }

}  // namespace esphome::fujitsu_general
