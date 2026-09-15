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
  // Bytes that arrived during the hand-off belong to neither owner.
  this->flush_input_();
  this->route_ = Route::ROUTE_LOCAL;
  ESP_LOGD(TAG, "UART routed to local consumers");
  this->disable_loop();
}

void UARTMux::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Mux:\n"
                "  Start local: %s\n"
                "  Route: %s",
                YESNO(this->start_local_), this->is_local() ? LOG_STR_LITERAL("local") : LOG_STR_LITERAL("bridge"));
}

void UARTMux::load_settings(bool dump_config) {
  ESP_LOGW(TAG, "load_settings() ignored; change the framing on the hardware UART instead");
}

void UARTMux::select_local() {
  if (this->route_ != Route::ROUTE_BRIDGE) {
    return;
  }
  ESP_LOGD(TAG, "Pausing bridge to route UART locally");
  this->bridge_->pause();
  this->route_ = Route::ROUTE_PENDING_LOCAL;
  this->enable_loop();
}

void UARTMux::select_bridge() {
  if (this->route_ == Route::ROUTE_BRIDGE) {
    return;
  }
  // While the pause is still pending the bridge's RX task may be inside
  // uart_read_bytes() on this port, and nothing local has run, so flush only a
  // completed hand-off.
  if (this->route_ == Route::ROUTE_LOCAL) {
    this->flush_input_();
  }
  this->route_ = Route::ROUTE_BRIDGE;
  ESP_LOGD(TAG, "UART routed to bridge");
  this->bridge_->resume();
  this->disable_loop();
}

void UARTMux::flush_input_() {
  uart_flush_input(static_cast<uart_port_t>(this->uart_->get_hw_serial_number()));
  // The driver flush leaves the UART component's one-byte peek cache in place.
  uint8_t discard;
  if (this->uart_->available() > 0) {
    this->uart_->read_byte(&discard);
  }
}

void UARTMux::write_array(const uint8_t *data, size_t len) {
  if (!this->is_local()) {
    ESP_LOGV(TAG, "Dropping %zu bytes: UART routed to bridge", len);
    return;
  }
  this->uart_->write_array(data, len);
}

}  // namespace esphome::uart_mux
#endif
