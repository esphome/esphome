#ifdef USE_NRF52
#ifdef USE_ARDUINO
#include <Adafruit_TinyUSB.h> // for Serial
#endif
#include "logger.h"
#include "esphome/core/log.h"

namespace esphome {
namespace logger {

#ifdef USE_ZEPHYR
// it must be inside namespace since there is duplicated macro EMPTY
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#endif

static const char *const TAG = "logger";

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
#ifdef USE_ARDUINO
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
        break;
      case UART_SELECTION_USB_CDC:
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
        break;
    }
#elif defined(USE_ZEPHYR)
    static const struct device *uart_dev = nullptr;
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart0));
        break;
      case UART_SELECTION_USB_CDC:
          uart_dev =  DEVICE_DT_GET_OR_NULL(zephyr_cdc_acm_uart);
          if(device_is_ready(uart_dev)) {
            usb_enable(NULL);
          }
        break;
    }
    if (!device_is_ready(uart_dev)) {
      ESP_LOGE(TAG, "%s is not ready.", get_uart_selection_());
    } else {
      uart_dev_ = uart_dev;
    }
#endif
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
}

#ifdef USE_ZEPHYR
void HOT Logger::write_msg_(const char *msg) {
  if(nullptr == uart_dev_) {
    return;
  }
  while(*msg) {
    uart_poll_out(uart_dev_, *msg);
    ++msg;
  }
  uart_poll_out(uart_dev_, '\n');
}
#endif

const char *const UART_SELECTIONS[] = {"UART0", "USB_CDC"};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[this->uart_]; }

}  // namespace logger
}  // namespace esphome

#endif
