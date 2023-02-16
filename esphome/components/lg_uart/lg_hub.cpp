// For itoa()
#include <stdlib.h>
#include <stdio.h>
#include "lg_hub.h"

namespace esphome {
namespace lg_uart {

/* Public */

bool LGUartHub::button_off() {
  ESP_LOGD(TAG, "Button off for screen_num: '%i'", this->screen_num_);

  // Power off is ka, 00
  std::string s = str_sprintf("ka %02x %02x\r", this->screen_num_, 00);
  this->parent_->write_array(std::vector<uint8_t>(s.begin(), s.end()));
  return true;
}

// TODO: this needs to get moved into switch
bool LGUartHub::button_off() {
  ESP_LOGD(TAG, "Button on for screen_num: '%i'", this->screen_num_);

  // Power off is ka, 00
  std::string s = str_sprintf("ka %02x %02x\r", this->screen_num_, 01);
  this->parent_->write_array(std::vector<uint8_t>(s.begin(), s.end()));
  return true;
}

/* Internal */

void LGUartHub::loop() {
  ESP_LOGCONFIG(TAG, "LOOP for screen_num: '%i'", this->screen_num_);
  this->button_off();
}

void LGUartHub::setup() {
  // this->encode_int(this->screen_num_, this->screen_num_enc);
  this->dump_config();
}

void LGUartHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Config for screen_num: '%i'");
  ESP_LOGCONFIG(TAG, "Uart baud rate '%i'", this->parent_->get_baud_rate());
}

}  // namespace lg_uart
}  // namespace esphome
