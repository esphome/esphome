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

  /*
  TODO: need to implement read uart.
  When command is set, either we'll get back NG or OK.
  */
  // Bytes as we read them
  uint8_t response[PACKET_LEN];
  uint8_t peeked;
  uint8_t idx = 0;

  // Note: this does not get called when uart debugging turned on!
  while (this->available()) {
    // Get the byte
    this->read_byte(&peeked);
    response[idx] = peeked;
    idx += 1;
    ESP_LOGD(TAG, "peeked[%i]: 0x%x \t %u \t [%c]", idx, peeked, peeked, peeked);

    // // If we have 5 bytes, we can start to process: the first 3 will be preamble, byte 4 will be the type and byte 5
    // // will be the value
    // if (idx == (PACKET_LEN - 1) && response[3] == 0x11) {
    //   ESP_LOGD(TAG, "EARLY EXIT; active bank!");
    //   break;
    // } else if (idx == (PACKET_LEN - 1)) {
    //   ESP_LOGD(TAG, "EARLY EXIT; unknown type. Start over!");
    //   idx = 0;
    // }
  }
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
