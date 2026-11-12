#ifdef USE_LIBRETINY
#include "logger.h"

#include "esphome/components/libretiny/core.h"
#ifdef USE_LIBRETINY_CRASH_HANDLER
#include "esphome/components/libretiny/crash_handler.h"
#endif

namespace esphome::logger {

ESPHOME_LOG_TAG(TAG, "logger");

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
#if defined(USE_LOGGER_UART_SELECTION_UART0) && LT_HW_UART0
    this->hw_serial_ = &Serial0;
    libretiny::ensure_serial_handle(0);
    Serial0.begin(this->baud_rate_);
#elif defined(USE_LOGGER_UART_SELECTION_UART1) && LT_HW_UART1
    this->hw_serial_ = &Serial1;
    libretiny::ensure_serial_handle(1);
    Serial1.begin(this->baud_rate_);
#elif defined(USE_LOGGER_UART_SELECTION_UART2) && LT_HW_UART2
    this->hw_serial_ = &Serial2;
    libretiny::ensure_serial_handle(2);
    Serial2.begin(this->baud_rate_);
#else
    this->hw_serial_ = &Serial;
    Serial.begin(this->baud_rate_);
    if (CONFIGURED_UART_SELECTION != UART_SELECTION_DEFAULT) {
      ESP_LOGW(TAG, "  The chosen logger UART port is not available on this board."
                    "The default port was used instead.");
    }
#endif
    // change lt_log() port to match default Serial
    lt_log_set_port(this->get_uart() - 1);
  }

  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
#ifdef USE_LIBRETINY_CRASH_HANDLER
  libretiny::crash_handler_log();
#endif
}

#ifdef USE_LIBRETINY_CRASH_HANDLER
bool crash_log_pending() { return libretiny::crash_handler_has_data(); }
void log_crash_to_client() { libretiny::crash_handler_log(); }
#endif

}  // namespace esphome::logger

#endif  // USE_LIBRETINY
