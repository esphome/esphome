#ifdef USE_ESP32
#include "logger.h"

#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_ESP_IDF)
#include <esp_log.h>
#endif  // USE_ESP32_FRAMEWORK_ARDUINO || USE_ESP_IDF

#ifdef USE_ESP_IDF
#include <driver/uart.h>

#ifdef USE_LOGGER_USB_SERIAL_JTAG
#include <driver/usb_serial_jtag.h>
#include <esp_vfs_dev.h>
#include <esp_vfs_usb_serial_jtag.h>
#endif

#include "freertos/FreeRTOS.h"
#include "esp_idf_version.h"

#include <cstdint>
#include <cstdio>
#include <fcntl.h>

#endif  // USE_ESP_IDF

#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

#ifdef USE_ESP_IDF

#ifdef USE_LOGGER_USB_SERIAL_JTAG
static void init_usb_serial_jtag_() {
  setvbuf(stdin, NULL, _IONBF, 0);  // Disable buffering on stdin

  // Minicom, screen, idf_monitor send CR when ENTER key is pressed
  esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
  // Move the caret to the beginning of the next line on '\n'
  esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

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
  esp_vfs_usb_serial_jtag_use_driver();
}
#endif

void init_uart(uart_port_t uart_num, uint32_t baud_rate, int tx_buffer_size) {
  uart_config_t uart_config{};
  uart_config.baud_rate = (int) baud_rate;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  uart_config.source_clk = UART_SCLK_DEFAULT;
#endif
  uart_param_config(uart_num, &uart_config);
  const int uart_buffer_size = tx_buffer_size;
  // Install UART driver using an event queue here
  uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 10, nullptr, 0);
}

#endif  // USE_ESP_IDF

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
#ifdef USE_ARDUINO
    switch (this->uart_) {
      case UART_SELECTION_UART0:
#if ARDUINO_USB_CDC_ON_BOOT
        this->hw_serial_ = &Serial0;
        Serial0.begin(this->baud_rate_);
#else
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
#endif
        break;
      case UART_SELECTION_UART1:
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
        break;
#ifdef USE_ESP32_VARIANT_ESP32
      case UART_SELECTION_UART2:
        this->hw_serial_ = &Serial2;
        Serial2.begin(this->baud_rate_);
        break;
#endif

#ifdef USE_LOGGER_USB_CDC
      case UART_SELECTION_USB_CDC:
        this->hw_serial_ = &Serial;
#if ARDUINO_USB_CDC_ON_BOOT
        Serial.setTxTimeoutMs(0);  // workaround for 2.0.9 crash when there's no data connection
#endif
        Serial.begin(this->baud_rate_);
        break;
#endif
    }
#endif  // USE_ARDUINO

#ifdef USE_ESP_IDF
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
#endif  // USE_ESP_IDF
  }

  global_logger = this;
#if defined(USE_ESP_IDF) || defined(USE_ESP32_FRAMEWORK_ARDUINO)
  esp_log_set_vprintf(esp_idf_log_vprintf_);
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE) {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  }
#endif  // USE_ESP_IDF || USE_ESP32_FRAMEWORK_ARDUINO

  ESP_LOGI(TAG, "Log initialized");
}

#ifdef USE_ESP_IDF
void HOT Logger::write_msg_(const char *msg) {
  if (
#if defined(USE_ESP32_VARIANT_ESP32S2)
      this->uart_ == UART_SELECTION_USB_CDC
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32H2)
      this->uart_ == UART_SELECTION_USB_SERIAL_JTAG
#elif defined(USE_ESP32_VARIANT_ESP32S3)
      this->uart_ == UART_SELECTION_USB_CDC || this->uart_ == UART_SELECTION_USB_SERIAL_JTAG
#else
      /* DISABLES CODE */ (false)  // NOLINT
#endif
  ) {
    puts(msg);
  } else {
    uart_write_bytes(this->uart_num_, msg, strlen(msg));
    uart_write_bytes(this->uart_num_, "\n", 1);
  }
}
#else
void HOT Logger::write_msg_(const char *msg) { this->hw_serial_->println(msg); }
#endif

const char *const UART_SELECTIONS[] = {
    "UART0",           "UART1",
#ifdef USE_ESP32_VARIANT_ESP32
    "UART2",
#endif
#ifdef USE_LOGGER_USB_CDC
    "USB_CDC",
#endif
#ifdef USE_LOGGER_USB_SERIAL_JTAG
    "USB_SERIAL_JTAG",
#endif
};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[this->uart_]; }

}  // namespace logger
}  // namespace esphome
#endif
