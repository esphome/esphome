#include "fujitsu_general_button.h"
#include "esphome/core/log.h"

namespace esphome::fujitsu_general {

static const char *const TAG = "fujitsu_general.button";

void FujitsuGeneralButton::press_action() { this->parent_->transmit_util(this->command_byte_); }

}  // namespace esphome::fujitsu_general
