#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

namespace esphome {

namespace logger {

/** Enum for logging UART selection
 *
 * Advanced configuration (pin selection, etc) is not supported.
 */
enum UARTSelection {
  UART_SELECTION_UART0 = 0,
  UART_SELECTION_UART1,
#ifdef ARDUINO_ARCH_ESP32
  UART_SELECTION_UART2
#endif
#ifdef ARDUINO_ARCH_ESP8266
      UART_SELECTION_UART0_SWAP
#endif
};

class Logger : public Component {
 public:
  explicit Logger(uint32_t baud_rate, size_t tx_buffer_size, UARTSelection uart);

  /// Manually set the baud rate for serial, set to 0 to disable.
  void set_baud_rate(uint32_t baud_rate);

  /// Set the buffer size that's used for constructing log messages. Log messages longer than this will be truncated.
  void set_tx_buffer_size(size_t tx_buffer_size);

  /// Get the UART used by the logger.
  UARTSelection get_uart() const;

  /// Set the global log level. Note: Use the ESPHOME_LOG_LEVEL define to also remove the logs from the build.
  void set_global_log_level(int log_level);
  int get_global_log_level() const { return this->global_log_level_; }

  /// Set the log level of the specified tag.
  void set_log_level(const std::string &tag, int log_level);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Set up this component.
  void pre_setup();
  void dump_config() override;

  int level_for(const char *tag);

  /// Register a callback that will be called for every log message sent
  void add_on_log_callback(std::function<void(int, const char *, const char *)> &&callback);

  float get_setup_priority() const override;

  int log_vprintf_(int level, const char *tag, const char *format, va_list args);  // NOLINT
#ifdef USE_STORE_LOG_STR_IN_FLASH
  int log_vprintf_(int level, const char *tag, const __FlashStringHelper *format, va_list args);  // NOLINT
#endif

 protected:
  void log_message_(int level, const char *tag, char *msg, int ret);

  uint32_t baud_rate_;
  std::vector<char> tx_buffer_;
  int global_log_level_{ESPHOME_LOG_LEVEL};
  UARTSelection uart_{UART_SELECTION_UART0};
  HardwareSerial *hw_serial_{nullptr};
  struct LogLevelOverride {
    std::string tag;
    int level;
  };
  std::vector<LogLevelOverride> log_levels_;
  CallbackManager<void(int, const char *, const char *)> log_callback_{};
};

extern Logger *global_logger;

}  // namespace logger

}  // namespace esphome
