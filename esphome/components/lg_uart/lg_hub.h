#pragma once

#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

#include "esphome/core/hal.h"

namespace esphome {
namespace lg_uart {

static const char *const TAG = "LGUart";
static const uint8_t PACKET_LEN = 9;

class LGUartHub : public Component, public uart::UARTDevice {
 public:
  /** Panel specific setup */
  void set_screen_number(int screen_num) { this->screen_num_ = screen_num; }
  int get_screen_number() { return this->screen_num_; }

  /** Generic commands */
  bool button_off();
  bool button_on();

  bool get_power_status() { return this->power_status_; }

  /* Component overrides */

  void loop() override;
  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  void encode_int(uint8_t x, char digits[]);
  int screen_num_ = -1;

  // User will give us an integer, we need to encode it for the uart protocol
  char screen_num_enc[2];

  int power_status_ = -1;
};

}  // namespace lg_uart
}  // namespace esphome
