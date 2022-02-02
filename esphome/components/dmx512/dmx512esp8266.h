#pragma once

#ifdef USE_ESP8266

#include "dmx512.h"

namespace esphome {
namespace dmx512 {

class DMX512ESP8266 : public DMX512 {
 public:
  DMX512ESP8266() = default;

  void sendBreak() override;

  void set_uart_num(int num) override {
    this->uart_idx_ = num;
  }
};

}  // namespace dmx512
}  // namespace esphome
#endif  // USE_ESP8266
