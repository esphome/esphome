#ifdef USE_ZEPHYR

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "logger.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>

namespace esphome::logger {

static const char *const TAG = "logger";

#ifdef USE_LOGGER_USB_CDC
void Logger::loop() {
  if (this->uart_ != UART_SELECTION_USB_CDC || this->uart_dev_ == nullptr) {
    return;
  }
  static bool opened = false;
  uint32_t dtr = 0;
  uart_line_ctrl_get(this->uart_dev_, UART_LINE_CTRL_DTR, &dtr);
  if (opened == static_cast<bool>(dtr)) {
    return;
  }
  if (!opened) {
    App.schedule_dump_config();
  }
  opened = !opened;
}
#endif

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    const struct device *uart_dev = nullptr;
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart0));
        break;
#ifdef USE_LOGGER_USB_CDC
      case UART_SELECTION_USB_CDC:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(cdc_acm_uart0));
        if (device_is_ready(uart_dev)) {
          usb_enable(nullptr);
        }
        break;
#endif
      default:
        break;
    }
    if (!device_is_ready(uart_dev)) {
      ESP_LOGE(TAG, "%s is not ready.", LOG_STR_ARG(get_uart_selection_()));
    } else {
      this->uart_dev_ = uart_dev;
    }
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
}

void HOT Logger::write_msg_(const char *msg, size_t len) { LOG_RAW("%.*s", static_cast<int>(len), msg); }

const LogString *Logger::get_uart_selection_() {
  switch (this->uart_) {
    case UART_SELECTION_UART0:
      return LOG_STR("UART0");
#ifdef USE_LOGGER_USB_CDC
    case UART_SELECTION_USB_CDC:
      return LOG_STR("USB_CDC");
#endif
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace esphome::logger

#endif
