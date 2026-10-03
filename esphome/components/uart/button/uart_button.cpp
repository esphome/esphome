#include "uart_button.h"
#include "esphome/core/log.h"

namespace esphome::uart {

static const char *const TAG = "uart.button";

void UARTButton::press_action() {
  ESP_LOGD(TAG, "'%s': Sending data", this->get_name().c_str());
  this->write_array_progmem(this->data_, this->data_len_);
}

void UARTButton::dump_config() { LOG_BUTTON("", "UART Button", this); }

}  // namespace esphome::uart
