#ifdef USE_ESP8266

#include "dmx512esp8266.h"
#include "esphome/core/log.h"
#include "uart_register.h"

namespace esphome {
namespace dmx512 {

static const char *TAG = "dmx512";

void DMX512ESP8266::sendBreak() {
    SET_PERI_REG_MASK(UART_CONF0(this->uart_idx_), UART_TXD_BRK);
    delayMicroseconds(this->break_len_);
    CLEAR_PERI_REG_MASK(UART_CONF0(this->uart_idx_), UART_TXD_BRK);
    delayMicroseconds(this->mab_len_);
}

}  // namespace dmx512
}  // namespace esphome
#endif  // USE_ESP8266
