#ifdef USE_ZEPHYR

#include "logger.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

void Logger::loop() {
  if (this->uart_ != UART_SELECTION_USB_CDC || nullptr == uart_dev_) {
    return;
  }
  static bool opened = false;
  uint32_t dtr = 0;
  uart_line_ctrl_get(uart_dev_, UART_LINE_CTRL_DTR, &dtr);

  /* Poll if the DTR flag was set, optional */
  if (opened == dtr) {
    return;
  }

  if (false == opened) {
    App.schedule_dump_config();
  }
  opened = !opened;
}

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    static const struct device *uart_dev = nullptr;
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart0));
        break;
      case UART_SELECTION_USB_CDC:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(cdc_acm_uart0));
        if (device_is_ready(uart_dev)) {
          usb_enable(NULL);
        }
        break;
    }
    if (!device_is_ready(uart_dev)) {
      ESP_LOGE(TAG, "%s is not ready.", get_uart_selection_());
    } else {
      uart_dev_ = uart_dev;
    }
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
}

void HOT Logger::write_msg_(const char *msg) {
#ifdef CONFIG_PRINTK
  printk("%s\n", msg);
#endif
  if (nullptr == uart_dev_) {
    return;
  }
  while (*msg) {
    uart_poll_out(uart_dev_, *msg);
    ++msg;
  }
  uart_poll_out(uart_dev_, '\n');
}

const char *const UART_SELECTIONS[] = {"UART0", "USB_CDC"};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[this->uart_]; }

}  // namespace logger
}  // namespace esphome

#endif
