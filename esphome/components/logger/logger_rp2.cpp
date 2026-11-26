#ifdef USE_RP2
#include "logger.h"
#include "esphome/core/defines.h"
#ifdef USE_RP2_CRASH_HANDLER
#include "esphome/components/rp2/crash_handler.h"
#endif
#include "esphome/core/log.h"

namespace esphome::logger {

ESPHOME_LOG_TAG(TAG, "logger");

#if defined(USE_RP2_CRASH_HANDLER) && defined(USE_LOGGER_UART_SELECTION_USB_CDC)
void Logger::setup() {
  if (this->hw_serial_ == nullptr || !crash_log_pending())
    return;
  // Report the crash each time a host opens the USB port (sets DTR)
  this->set_interval(500, [opened_before = false]() mutable {
    const bool opened = Serial.dtr();
    if (opened && !opened_before)
      log_crash_to_client();
    opened_before = opened;
  });
}
#endif

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
#if defined(USE_LOGGER_UART_SELECTION_USB_CDC)
    this->hw_serial_ = &Serial;
    Serial.begin(this->baud_rate_);
#elif defined(USE_LOGGER_UART_SELECTION_UART1)
    this->hw_serial_ = &Serial2;
    Serial2.begin(this->baud_rate_);
#else
    this->hw_serial_ = &Serial1;
    Serial1.begin(this->baud_rate_);
#endif
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
  // Nobody has a USB port open this early; setup() reports the crash once a host opens it
#if defined(USE_RP2_CRASH_HANDLER) && !defined(USE_LOGGER_UART_SELECTION_USB_CDC)
  rp2::crash_handler_log();
#endif
}

#ifdef USE_CRASH_HANDLER
bool crash_log_pending() { return rp2::crash_handler_has_data(); }
void log_crash_to_client() { rp2::crash_handler_log(); }
#endif

}  // namespace esphome::logger
#endif  // USE_RP2
