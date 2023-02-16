#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace lg_uart {
static const char *const TAG = "LGUart";
class LGUartHub : public Component, public uart::UARTDevice {
 public:
  void set_screen_number(int screen_num) { this->screen_num_ = screen_num; }
  int get_screen_number() { return this->screen_num_; }

  /** Press the OFF button. */
  bool button_off();
  bool button_on();

  bool get_power_status() { return this->power_status_; }

  /* Component overrides */

  void loop() override;
  // void update() override;
  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  int screen_num_ = -1;
  int power_status_ = -1;
};

}  // namespace lg_uart
}  // namespace esphome
