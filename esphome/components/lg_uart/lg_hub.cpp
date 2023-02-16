// For itoa()
#include <stdlib.h>
#include <stdio.h>
#include "lg_hub.h"

namespace esphome {
namespace lg_uart {

/* Public */

bool LGUartHub::button_off() {
  ESP_LOGD(TAG, "Button off (mute payload) for screen_num: '%i'", this->screen_num_);

  // For now, just use the MUTE packet for set 1
  //  [0x6B, 0x65, 0x20, 0x30, 0x31, 0x20, 0x30, 0x30, 0x0D]

  uint8_t pkt[] = {0x6B, 0x65, 0x20, 0x30, 0x31, 0x20, 0x30, 0x30, 0x0d};

  this->parent_->write_array(pkt, PACKET_LEN);
  return true;
}

// TODO: this needs to get moved into switch
bool LGUartHub::button_on() {
  ESP_LOGCONFIG(TAG, "Button off for screen_num: '%i'", this->screen_num_);
  return true;
}

/* Internal */

void LGUartHub::loop() {
  ESP_LOGCONFIG(TAG, "LOOP for screen_num: '%i'", this->screen_num_);
  char test[2];
  this->encode_int(this->screen_num_, test);
  ESP_LOGCONFIG(TAG, "LOOP for screen_num: '%x %x'", test[0], test[1]);
  // this->button_off();
}

void LGUartHub::setup() { this->dump_config(); }

void LGUartHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Config for screen_num: '%i'", this->screen_num_);
  ESP_LOGCONFIG(TAG, "Uart available? '%i'", this->parent_->available());
  ESP_LOGCONFIG(TAG, "Uart baud rate '%i'", this->parent_->get_baud_rate());
}

/*
  We need a function to encode integer values for uart purposes.
  This function will be used for screen ID and other values like volume / brightness

  This is because numbers like `01` need to get encoded as the ascii 0 and the ascii 1
  E.G.:
    User wants volume level 16. That's 0x10.
    We need to transmit the ascii chars 01 aka 0x31 0x30
    Basically a float -> int -> ascii -> hex pipe
*/
void LGUartHub::encode_int(uint8_t x, char digits[]) {
  char num[3];
  itoa(x, num, 16);  // I could not figure out a better way to do zero padding on the input so values like
  //  7 would get transformed into the string and then transmitted as 0x30 0x37
  if ((int) x < 10) {
    digits[0] = 0x30;
    digits[1] = num[0];
  } else {
    digits[0] = num[0];
    digits[1] = num[1];
  }
}

}  // namespace lg_uart
}  // namespace esphome
