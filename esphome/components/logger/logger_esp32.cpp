#ifdef USE_ESP32
#include "logger.h"

#include <esp_log.h>

#include <driver/uart.h>

#ifdef USE_LOGGER_USB_SERIAL_JTAG
#include <driver/usb_serial_jtag.h>
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#include <esp_vfs_dev.h>
#include <esp_vfs_usb_serial_jtag.h>
#else
#include <driver/usb_serial_jtag_vfs.h>
#endif
#endif

#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"

#include <fcntl.h>
#include <cstdint>
#include <cstdio>

#include "esphome/core/log.h"

namespace esphome::logger {

static const char *const TAG = "logger";

#ifdef USE_LOGGER_USB_SERIAL_JTAG
static void init_usb_serial_jtag_() {
  setvbuf(stdin, NULL, _IONBF, 0);  // Disable buffering on stdin

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
  // Minicom, screen, idf_monitor send CR when ENTER key is pressed
  esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
  // Move the caret to the beginning of the next line on '\n'
  esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
#else
  // Minicom, screen, idf_monitor send CR when ENTER key is pressed
  usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
  // Move the caret to the beginning of the next line on '\n'
  usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
#endif

  // Enable non-blocking mode on stdin and stdout
  fcntl(fileno(stdout), F_SETFL, 0);
  fcntl(fileno(stdin), F_SETFL, 0);

  usb_serial_jtag_driver_config_t usb_serial_jtag_config{};
  usb_serial_jtag_config.rx_buffer_size = 512;
  usb_serial_jtag_config.tx_buffer_size = 512;

  esp_err_t ret = ESP_OK;
  // Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes
  ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
  if (ret != ESP_OK) {
    return;
  }

  // Tell vfs to use usb-serial-jtag driver
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
  esp_vfs_usb_serial_jtag_use_driver();
#else
  usb_serial_jtag_vfs_use_driver();
#endif
}
#endif

void init_uart(uart_port_t uart_num, uint32_t baud_rate, int tx_buffer_size) {
  uart_config_t uart_config{};
  uart_config.baud_rate = (int) baud_rate;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_DEFAULT;
  uart_param_config(uart_num, &uart_config);
  const int uart_buffer_size = tx_buffer_size;
  // Install UART driver using an event queue here
  uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 10, nullptr, 0);
}

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    this->uart_num_ = UART_NUM_0;
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        this->uart_num_ = UART_NUM_0;
        init_uart(this->uart_num_, baud_rate_, tx_buffer_size_);
        break;
      case UART_SELECTION_UART1:
        this->uart_num_ = UART_NUM_1;
        init_uart(this->uart_num_, baud_rate_, tx_buffer_size_);
        break;
#ifdef USE_ESP32_VARIANT_ESP32
      case UART_SELECTION_UART2:
        this->uart_num_ = UART_NUM_2;
        init_uart(this->uart_num_, baud_rate_, tx_buffer_size_);
        break;
#endif
#ifdef USE_LOGGER_USB_CDC
      case UART_SELECTION_USB_CDC:
        break;
#endif
#ifdef USE_LOGGER_USB_SERIAL_JTAG
      case UART_SELECTION_USB_SERIAL_JTAG:
        init_usb_serial_jtag_();
        break;
#endif
    }
  }

  global_logger = this;
  esp_log_set_vprintf(esp_idf_log_vprintf_);
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE) {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  }

  ESP_LOGI(TAG, "Log initialized");
}

void HOT Logger::write_msg_(const char *msg, size_t len) {
  // Length is now always passed explicitly - no strlen() fallback needed

#if defined(USE_LOGGER_UART_SELECTION_USB_CDC) || defined(USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG)
  // USB CDC/JTAG - single write including newline (already in buffer)
  // Use fwrite to stdout which goes through VFS to USB console
  //
  // Note: These defines indicate the user's YAML configuration choice (hardware_uart: USB_CDC/USB_SERIAL_JTAG).
  // They are ONLY defined when the user explicitly selects USB as the logger output in their config.
  // This is compile-time selection, not runtime detection - if USB is configured, it's always used.
  // There is no fallback to regular UART if "USB isn't connected" - that's the user's responsibility
  // to configure correctly for their hardware. This approach eliminates runtime overhead.
  fwrite(msg, 1, len, stdout);
#else
  // Regular UART - single write including newline (already in buffer)
  uart_write_bytes(this->uart_num_, msg, len);
#endif
}

const LogString *Logger::get_uart_selection_() {
  switch (this->uart_) {
    case UART_SELECTION_UART0:
      return LOG_STR("UART0");
    case UART_SELECTION_UART1:
      return LOG_STR("UART1");
#ifdef USE_ESP32_VARIANT_ESP32
    case UART_SELECTION_UART2:
      return LOG_STR("UART2");
#endif
#ifdef USE_LOGGER_USB_CDC
    case UART_SELECTION_USB_CDC:
      return LOG_STR("USB_CDC");
#endif
#ifdef USE_LOGGER_USB_SERIAL_JTAG
    case UART_SELECTION_USB_SERIAL_JTAG:
      return LOG_STR("USB_SERIAL_JTAG");
#endif
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace esphome::logger
#endif
