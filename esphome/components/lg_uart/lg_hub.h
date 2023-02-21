#pragma once
#include <map>

#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

#include "esphome/core/hal.h"
#include "lg_uart_child.h"

namespace esphome {
namespace lg_uart {

static const char *const TAG = "LGUart";
// LG docs indicate that reply will be no longer than 9 bytes
static const uint8_t PACKET_LEN = 10;

class LGUartHub : public Component, public uart::UARTDevice {
 public:
  /** Panel specific setup */
  void set_screen_number(int screen_num) { this->screen_num_ = screen_num; }
  int get_screen_number() { return this->screen_num_; }

  /** Generic commands */
  bool send_cmd(char cmd_code[2], int data);

  /* Component overrides */

  void loop() override;
  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  /** Register a `LGUartClient` child component. */
  void register_child(LGUartClient *obj, std::string cmd_char);

 protected:
  int screen_num_ = -1;
  char screen_num_chars[3] = {0, 0, 0};

  // Index the children by the second character of the command.
  // E.G.: command codes `ka`, `ke`, `kd` will result in children `a`, `e`, `d`.
  std::map<char, LGUartClient *> children_;
};

}  // namespace lg_uart
}  // namespace esphome
