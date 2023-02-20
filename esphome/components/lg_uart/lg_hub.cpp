// For itoa()
#include <stdlib.h>
#include <stdio.h>

#include "lg_hub.h"

#include "lg_uart_child.h"

namespace esphome {
namespace lg_uart {

/* Public */

bool LGUartHub::send_cmd(char cmd_code[2], int data, uint8_t reply[PACKET_LEN]) {
  ESP_LOGD(TAG, "send_cmd(%s). data: '%i' for screen: '%i'", cmd_code, data, this->screen_num_);
  std::string s = str_sprintf("%02s %02x %02x\r", cmd_code, this->screen_num_, data);
  this->parent_->write_array(std::vector<uint8_t>(s.begin(), s.end()));

  uint8_t peeked;
  uint8_t idx = 0;

  bool stream_valid = false;

  // Note: this does not get called when uart debugging turned on!
  while (this->available()) {
    // Get the byte
    this->read_byte(&peeked);
    // LG replies always have the second command char at the beginning. Buffer may have noise
    //   so we read until we get expected then start storing
    if (!stream_valid && (peeked != cmd_code[1])) {
      /*
        I should probably refactor this as i'm hitting this code A LOT.

        I need to keep track of the children, specifically by the second character of the command.
        Each child should get a process_packet() or similar.
        That way I don't have to ignore anything, I can dispatch all valid incoming packets
      */
      ESP_LOGD(TAG, "send_cmd(%s) - IGNORE peeked[%i]: 0x%x \t %u \t [%c]", cmd_code, idx, peeked, peeked, peeked);
      continue;
    } else if (!stream_valid && (peeked == cmd_code[1])) {
      stream_valid = true;
    }
    reply[idx] = peeked;
    ESP_LOGD(TAG, "send_cmd(%s). peeked[%i]: 0x%x \t %u \t [%c]", cmd_code, idx, peeked, peeked, peeked);
    idx += 1;

    if (idx == PACKET_LEN) {
      // OK
      if (reply[5] == 0x4f && reply[6] == 0x4b) {
        ESP_LOGD(TAG, "send_cmd(%s). got OK");
        return true;
      } else {
        ESP_LOGW(TAG, "send_cmd(%s). got NOT OK");
        return false;
      }
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

  ESP_LOGCONFIG(TAG, "  Child components (%d):", this->children_.size());

  // Dump the cmd char and the name of the thing responding to it
  for (const auto [key, value] : this->children_) {
    ESP_LOGCONFIG(TAG, "    -  [%c] => %s", key, value->describe().c_str());
  }
}

void LGUartHub::register_child(LGUartClient *obj, std::string cmd_char) {
  this->children_[cmd_char.at(0)] = obj;
  obj->set_parent(this);
}

}  // namespace lg_uart
}  // namespace esphome
