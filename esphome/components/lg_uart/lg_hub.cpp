// For itoa()
#include <stdlib.h>
#include <stdio.h>
#include "lg_hub.h"

#include "lg_uart_child.h"

namespace esphome {
namespace lg_uart {

/* Public */

void LGUartHub::send_cmd(char cmd[2], int data) {
  ESP_LOGD(TAG, "Sending cmd: '%s' data: '%i' for screen: '%i'", cmd, data, this->screen_num_);
  std::string s = str_sprintf("%02s %02x %02x\r", cmd, this->screen_num_, data);
  this->parent_->write_array(std::vector<uint8_t>(s.begin(), s.end()));
}

/* Internal */

void LGUartHub::loop() {
  // ESP_LOGCONFIG(TAG, "LOOP for screen_num: '%i'", this->screen_num_);
}

void LGUartHub::setup() { this->dump_config(); }

void LGUartHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Config for screen_num: '%i'", this->screen_num_);
  ESP_LOGCONFIG(TAG, "Uart baud rate '%i'", this->parent_->get_baud_rate());
}

void LGUartHub::register_child(LGUartClient *obj) { obj->set_parent(this); }

}  // namespace lg_uart
}  // namespace esphome
