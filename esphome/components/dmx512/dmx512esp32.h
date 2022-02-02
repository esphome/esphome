#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "dmx512.h"
#include "esp32-hal-matrix.h"

namespace esphome {
namespace dmx512 {

class DMX512ESP32 : public DMX512 {
 public:
  DMX512ESP32() = default;

  void sendBreak() override;

  void set_uart_num(int num) override {
    if(num == 0) {
        this->uart_idx_ = U0TXD_OUT_IDX;
    } else if(num == 1) {
        this->uart_idx_ = U1TXD_OUT_IDX;
    } else if(num == 2) {
        this->uart_idx_ = U2TXD_OUT_IDX;
    }
  }
};

}  // namespace dmx512
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
