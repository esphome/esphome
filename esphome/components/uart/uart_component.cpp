#include "uart_component.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart";

bool UARTComponent::check_read_timeout_(size_t len) {
  if (this->available() >= int(len))
    return true;

  uint32_t start_time = millis();
  while (this->available() < int(len)) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Reading from UART timed out at byte %u!", this->available());
      return false;
    }
    yield();
  }
  return true;
}

void UARTComponent::set_rx_full_threshold_ms(uint8_t time) {
  uint8_t bytelength = this->data_bits_ + this->stop_bits_ + 1;
  if (this->parity_ != UARTParityOptions::UART_CONFIG_PARITY_NONE)
    bytelength += 1;
  int32_t val = clamp<int32_t>((this->baud_rate_ / (bytelength * 1000 / time)) - 1, 1, 120);
  this->set_rx_full_threshold(val);
}

#ifdef USE_UART_DEBUGGER
  std::string UARTComponent::get_debug_settings_string(bool debug_add_settings_) {
    // return settings string or empty if not desired
    if (!debug_add_settings)
      return "";
    this->debug_add_settings_ = true;
    std::string res = "|" + std::to_string(this->get_baud_rate()) + ":";
    res += std::to_string(this->get_data_bits()) + ":";
    res += std::to_string(this->get_stop_bits()) + ":";
    switch(this->get_parity()) {
      case 0:
        res += "none|";
        return res;
      case 1:
        res += "even|";
        return res;
      case 2:
        res += "odd|";
        return res;
    }
    return res;
  }
#endif

}  // namespace uart
}  // namespace esphome
