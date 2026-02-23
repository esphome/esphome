#include "fujitsu_general_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fujitsu_general {

static const char *const TAG = "fujitsu_general.button";

void FujitsuGeneralButton::dump_config() {
  ESP_LOGCONFIG(TAG, "Fujitsu General Button '%s'", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Command Byte: 0x%02X", this->command_byte_);
  ESP_LOGCONFIG(TAG, "  Parent Climate: %s", this->parent_->get_name().c_str());
}

void FujitsuGeneralButton::press_action() { this->parent_->transmit_util(this->command_byte_); }

}  // namespace fujitsu_general
}  // namespace esphome
