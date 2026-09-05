#ifdef USE_ESP32
#include "logger.h"

#include "esphome/components/esp32/crash_handler.h"
#include <esp_log.h>

#include <driver/uart.h>

#ifdef USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG
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

#ifdef USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG
static void init_usb_serial_jtag() {
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
  // The logger only writes to UART, never reads, so use the minimum RX buffer.
  // ESP-IDF requires rx_buffer_size > UART_HW_FIFO_LEN (128 bytes).
  const int min_rx_buffer_size = UART_HW_FIFO_LEN(uart_num) + 1;
  uart_driver_install(uart_num, min_rx_buffer_size, tx_buffer_size, 0, nullptr, 0);
}

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    this->uart_num_ = UART_NUM_0;
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        this->uart_num_ = UART_NUM_0;
        init_uart(this->uart_num_, baud_rate_, ESPHOME_LOGGER_TX_BUFFER_SIZE);
        break;
      case UART_SELECTION_UART1:
        this->uart_num_ = UART_NUM_1;
        init_uart(this->uart_num_, baud_rate_, ESPHOME_LOGGER_TX_BUFFER_SIZE);
        break;
#ifdef USE_ESP32_VARIANT_ESP32
      case UART_SELECTION_UART2:
        this->uart_num_ = UART_NUM_2;
        init_uart(this->uart_num_, baud_rate_, ESPHOME_LOGGER_TX_BUFFER_SIZE);
        break;
#endif
#ifdef USE_LOGGER_USB_CDC
      case UART_SELECTION_USB_CDC:
        break;
#endif
#ifdef USE_LOGGER_USB_SERIAL_JTAG
      case UART_SELECTION_USB_SERIAL_JTAG:
#ifdef USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG
        init_usb_serial_jtag();
#endif
        break;
#endif
    }
  }

  global_logger = this;
  esp_log_set_vprintf(esp_idf_log_vprintf_);

  ESP_LOGI(TAG, "Log initialized");
#ifdef USE_ESP32_CRASH_HANDLER
  esp32::crash_handler_log();
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

#ifdef USE_ESP32_LOG_V2
#include <esp_private/log_message.h>
#include <esp_log_write.h>
#include <esp_rom_sys.h>

namespace esphome::logger {

// IDF levels NONE=0 E=1 W=2 I=3 D=4 V=5; ESPHome inserts CONFIG at 4
static inline uint8_t idf_to_esphome_level(uint8_t idf_level) {
  if (idf_level <= 3)
    return idf_level;
  return idf_level >= 5 ? ESPHOME_LOG_LEVEL_VERBOSE : ESPHOME_LOG_LEVEL_DEBUG;
}

// Console output without the logger hook: early boot and constrained env
// (fwrite locks crash during PHY init on USB JTAG). Cold path.
static void __attribute__((noinline)) esp_log_format_direct(esp_log_msg_t *message) {
  // Constrained-env stacks are small; enough for IDF's own one-liners
  char stack_buf[256];
  LogBuffer buf{stack_buf, sizeof(stack_buf)};
  buf.write_header(idf_to_esphome_level(message->config.opts.log_level), message->tag ? message->tag : "esp-idf", 0,
                   nullptr);
  buf.format_body(message->format, message->args);
  esp_rom_printf("%s\n", stack_buf);
}

}  // namespace esphome::logger

extern "C" {
// Replaces liblog's formatter, which calls the vprintf hook 3x per message
// (header, body, newline). --wrap because a strong definition collides:
// liblog resolves the symbol within its own archive. Not IRAM_ATTR: cache-off
// callers bypass esp_log() under CONSTRAINED_ENV_SAFE=n.
void __wrap_esp_log_format(esp_log_msg_t *message) {  // NOLINT
  extern vprintf_like_t esp_log_vprint_func;
  if (esp_log_vprint_func == &esphome::esp_idf_log_vprintf_) [[likely]] {
    if (message->config.opts.constrained_env) [[unlikely]] {
      esphome::logger::esp_log_format_direct(message);
      return;
    }
    // Call the logger directly with V2's separate tag and severity so lines
    // render as e.g. "[E][wifi]:"; the hook signature cannot carry them.
    // global_logger is set before the hook is installed (pre_setup above).
    esphome::logger::global_logger->log_vprintf_(esphome::logger::idf_to_esphome_level(message->config.opts.log_level),
                                                 message->tag ? message->tag : "esp-idf", 0, message->format,
                                                 message->args);
    return;
  }
  extern int vprintf(const char *, __gnuc_va_list);  // NOLINT
  if (esp_log_vprint_func == &vprintf || message->config.opts.constrained_env) {
    // No hook yet (early boot) or constrained env: direct console output
    esphome::logger::esp_log_format_direct(message);
    return;
  }
  // Custom hook via esp_log_set_vprintf: forward the body as-is
  esp_log_vprint_func(message->format, message->args);
}
}  // extern "C"
#endif  // USE_ESP32_LOG_V2
#endif
