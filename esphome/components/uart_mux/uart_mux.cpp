#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "uart_mux.h"
#include "esphome/core/log.h"

#include "driver/uart.h"

namespace esphome::uart_mux {

static const char *const TAG = "uart_mux";

void UARTMux::setup() {
  // Mirror the framing so consumers reading it from their parent see the real values.
  this->baud_rate_ = this->uart_->get_baud_rate();
  this->data_bits_ = this->uart_->get_data_bits();
  this->stop_bits_ = this->uart_->get_stop_bits();
  this->parity_ = this->uart_->get_parity();

  if (this->start_local_) {
    this->select_local();
  } else {
    // loop() only completes hand-offs; the bridge keeps the UART until an action.
    this->disable_loop();
  }
}

void UARTMux::loop() {
  if (!this->bridge_->is_paused()) {
    return;
  }
  this->handoff_pending_ = false;
  // Bytes that arrived during the hand-off belong to neither owner.
  this->flush_input_();
  this->local_active_ = true;
  ESP_LOGD(TAG, "UART routed to local consumers");
  this->disable_loop();
}

void UARTMux::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Mux:\n"
                "  Start local: %s\n"
                "  Route: %s",
                YESNO(this->start_local_), this->local_active_ ? LOG_STR_LITERAL("local") : LOG_STR_LITERAL("bridge"));
}

void UARTMux::select_local() {
  if (this->local_active_ || this->handoff_pending_) {
    return;
  }
  ESP_LOGD(TAG, "Pausing bridge to route UART locally");
  this->bridge_->pause();
  this->handoff_pending_ = true;
  this->enable_loop();
}

void UARTMux::select_bridge() {
  if (!this->local_active_ && !this->handoff_pending_) {
    return;
  }
  this->handoff_pending_ = false;
  this->local_active_ = false;
  this->flush_input_();
  ESP_LOGD(TAG, "UART routed to bridge");
  this->bridge_->resume();
  this->disable_loop();
}

void UARTMux::flush_input_() { uart_flush_input(static_cast<uart_port_t>(this->uart_->get_hw_serial_number())); }

void UARTMux::write_array(const uint8_t *data, size_t len) {
  if (!this->local_active_) {
    ESP_LOGV(TAG, "Dropping %zu bytes: UART routed to bridge", len);
    return;
  }
  this->uart_->write_array(data, len);
}

bool UARTMux::peek_byte(uint8_t *data) { return this->local_active_ && this->uart_->peek_byte(data); }

bool UARTMux::read_array(uint8_t *data, size_t len) {
  return this->local_active_ && this->uart_->read_array(data, len);
}

size_t UARTMux::available() { return this->local_active_ ? this->uart_->available() : 0; }

uart::UARTFlushResult UARTMux::flush() {
  if (!this->local_active_) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
  }
  return this->uart_->flush();
}

}  // namespace esphome::uart_mux
#endif
