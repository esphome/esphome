#ifdef USE_NRF52
#ifdef USE_ARDUINO
#include <Adafruit_TinyUSB.h> // for Serial
#endif
#include "logger.h"
#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
        break;
#if defined(PIN_SERIAL2_RX) && defined(PIN_SERIAL2_TX)
      case UART_SELECTION_UART1:
        this->hw_serial_ = &Serial2;
        Serial2.begin(this->baud_rate_);
        break;
#endif
      case UART_SELECTION_USB_CDC:
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
        break;
    }
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
}

const char *const UART_SELECTIONS[] = {"USB_CDC"};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[this->uart_]; }

}  // namespace logger
}  // namespace esphome

#endif
