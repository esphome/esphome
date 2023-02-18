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

  bool stream_valid = false;

  // Note: this does not get called when uart debugging turned on!
  while (this->available()) {
    // Get the byte
    this->read_byte(&peeked);
    // LG replies always have the second command char at the beginning. Buffer may have noise
    //   so we read until we get expected then start storing
    if (!stream_valid && peeked != cmd[1]) {
      ESP_LOGD(TAG, "IGNORE peeked[%i]: 0x%x \t %u \t [%c]", idx, peeked, peeked, peeked);
      continue;
    } else if (!stream_valid && peeked == cmd[1]) {
      stream_valid = true;
    }
    response[idx] = peeked;
    ESP_LOGD(TAG, "peeked[%i]: 0x%x \t %u \t [%c]", idx, peeked, peeked, peeked);
    idx += 1;
    // TODO; confirm set number
    /*
      If the screen is alive, we'll get back a packet like so

      [e][ ][0][1][ ][O][K][0][x]

      We are looking for OK or NG. The last byte is the current status

      TODO: return OK/NG + value back to caller
        (I think i'll need to add two more function args)

    */
    if (idx == PACKET_LEN) {
      ESP_LOGD(TAG, "end of packet");
      // OK
      if (response[5] == 0x4f && response[6] == 0x4b) {
        ESP_LOGD(TAG, "got OK");
      } else {
        ESP_LOGW(TAG, "got NOT OK");
      }
      return;
    }
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
