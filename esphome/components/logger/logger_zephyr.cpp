#ifdef USE_ZEPHYR

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "logger.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

namespace esphome::logger {

static const char *const TAG = "logger";

void Logger::loop() {
#ifdef USE_LOGGER_USB_CDC
  if (this->uart_ != UART_SELECTION_USB_CDC || nullptr == this->uart_dev_) {
    return;
  }
  static bool opened = false;
  uint32_t dtr = 0;
  uart_line_ctrl_get(this->uart_dev_, UART_LINE_CTRL_DTR, &dtr);

  /* Poll if the DTR flag was set, optional */
  if (opened == dtr) {
    return;
  }

  if (!opened) {
    App.schedule_dump_config();
  }
  opened = !opened;
#endif
  this->process_messages_();
}

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    static const struct device *uart_dev = nullptr;
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart0));
        break;
      case UART_SELECTION_UART1:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart1));
        break;
#ifdef USE_LOGGER_USB_CDC
      case UART_SELECTION_USB_CDC:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(cdc_acm_uart0));
        if (device_is_ready(uart_dev)) {
          usb_enable(nullptr);
        }
        break;
#endif
    }
    if (!device_is_ready(uart_dev)) {
      ESP_LOGE(TAG, "%s is not ready.", get_uart_selection_());
    } else {
      this->uart_dev_ = uart_dev;
    }
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
}

void HOT Logger::write_msg_(const char *msg) {
#ifdef CONFIG_PRINTK
  printk("%s\n", msg);
#endif
  if (nullptr == this->uart_dev_) {
    return;
  }
  while (*msg) {
    uart_poll_out(this->uart_dev_, *msg);
    ++msg;
  }
  uart_poll_out(this->uart_dev_, '\n');
}

const char *const UART_SELECTIONS[] = {"UART0", "UART1", "USB_CDC"};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[this->uart_]; }

}  // namespace esphome::logger

#endif
