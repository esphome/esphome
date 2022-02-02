#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "dmx512esp32.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dmx512 {

static const char *TAG = "dmx512";

void DMX512ESP32::sendBreak() {
  uint8_t num = this->tx_pin_->get_pin();
  pinMatrixOutDetach(num, false, false);
  pinMode(num, OUTPUT);
  digitalWrite(num, LOW);
  delayMicroseconds(this->break_len_);
  digitalWrite(num, HIGH);
  delayMicroseconds(this->mab_len_);
  pinMatrixOutAttach(num, this->uart_idx_, false, false);
}

}  // namespace dmx512
}  // namespace esphome
#endif  // USE_ESP32_FRAMEWORK_ARDUINO
