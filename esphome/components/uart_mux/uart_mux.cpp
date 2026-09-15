#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "uart_mux.h"
#include "esphome/core/log.h"

#include "driver/uart.h"

namespace esphome::uart_mux {

static const char *const TAG = "uart_mux";

void UARTMux::setup() {
  // A failed UART never assigned its port; nothing behind the mux can work.
  if (this->uart_->is_failed()) {
    ESP_LOGE(TAG, "UART parent failed; aborting");
    this->mark_failed();
    return;
  }

  this->settings_ = {
      this->uart_->get_baud_rate(),      this->uart_->get_rx_full_threshold(), this->uart_->get_rx_timeout(),
      this->uart_->get_rx_buffer_size(), this->uart_->get_data_bits(),         this->uart_->get_stop_bits(),
      this->uart_->get_parity(),
  };
  this->apply_settings_();

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
                YESNO(this->start_local_),
                this->route_ == Route::ROUTE_LOCAL           ? LOG_STR_LITERAL("local")
                : this->route_ == Route::ROUTE_PENDING_LOCAL ? LOG_STR_LITERAL("pending local")
                                                             : LOG_STR_LITERAL("bridge"));
}

void UARTMux::load_settings(bool dump_config) {
  if (!this->load_settings_warned_) {
    this->load_settings_warned_ = true;
    ESP_LOGW(TAG, "load_settings() ignored; change the framing on the hardware UART instead");
  }
  // Undo whatever the caller set on us. Not re-sampled from the live UART, whose
  // fields carry the host's line coding while the bridge owns the bus.
  this->apply_settings_();
}

void UARTMux::apply_settings_() {
  this->baud_rate_ = this->settings_.baud_rate;
  this->data_bits_ = this->settings_.data_bits;
  this->stop_bits_ = this->settings_.stop_bits;
  this->parity_ = this->settings_.parity;
  this->rx_full_threshold_ = this->settings_.rx_full_threshold;
  this->rx_timeout_ = this->settings_.rx_timeout;
  this->rx_buffer_size_ = this->settings_.rx_buffer_size;
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
  // A bridge that failed setup() has no worker tasks; handing it the bus would kill
  // the UART in both directions.
  if (this->bridge_->is_failed()) {
    ESP_LOGW(TAG, "Bridge failed; keeping the UART routed locally");
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
  // Drain the UART component's one-byte peek cache first: the driver flush does not
  // clear it, and draining afterwards could discard a freshly arrived byte instead.
  uint8_t discard;
  if (this->uart_->available() > 0) {
    this->uart_->read_byte(&discard);
  }
  uart_flush_input(static_cast<uart_port_t>(this->uart_->get_hw_serial_number()));
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
