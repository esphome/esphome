

#include "dmx.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include "driver/uart.h"
#endif

namespace esphome::dmx {

ESPHOME_LOG_TAG(TAG, "dmx");

void DMXComponent::setup() {
  this->dmx_port_ = static_cast<dmx_port_t>(this->uart_num_);
  this->rx_data_[0] = 0;  // null start code
  this->tx_data_[0] = 0;  // null start code

#ifdef USE_ESP32
  if (uart_is_driver_installed(static_cast<uart_port_t>(this->uart_num_))) {
    ESP_LOGE(TAG,
             "UART%d is already in use. "
             "Check logger and UART configuration.",
             this->uart_num_);
    this->mark_failed();
    return;
  }
#endif

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {{1, "ESPHome DMX"}};

  if (!dmx_driver_install(this->dmx_port_, &config, personalities, 1)) {
    ESP_LOGE(TAG, "Failed to install DMX driver on UART%d", this->uart_num_);
    this->mark_failed();
    return;
  }

  if (!dmx_set_pin(this->dmx_port_, this->tx_pin_, this->rx_pin_, this->rts_pin_)) {
    ESP_LOGE(TAG, "Failed to configure DMX pins on UART%d", this->uart_num_);
    dmx_driver_delete(this->dmx_port_);
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "DMX started on UART%d (TX=%d RX=%d RTS=%d)", this->uart_num_, this->tx_pin_, this->rx_pin_,
           this->rts_pin_);
}

void DMXComponent::loop() {
  const bool want_rx = (this->mode_ != DMXMode::TX);
  const bool want_tx = (this->mode_ != DMXMode::RX) && this->tx_enabled_;

  bool got_rx = false;
  if (want_rx) {
    dmx_packet_t packet{};
    const size_t received_size = dmx_receive(this->dmx_port_, &packet, 0);
    if (received_size > 0) {
      if (packet.err == DMX_OK && !packet.is_rdm && packet.sc == 0x00) {
        if (received_size > DMX_PACKET_SIZE) {
          ESP_LOGW(TAG, "Ignoring oversized DMX packet (%u bytes)", static_cast<unsigned>(received_size));
        } else {
          const size_t bytes_read = dmx_read(this->dmx_port_, this->rx_data_, received_size);
          if (bytes_read == received_size) {
            this->publish_inputs_();
            this->last_rx_ms_ = millis();
            got_rx = true;
          } else {
            ESP_LOGW(TAG, "Incomplete DMX read: expected %u bytes, received %u", static_cast<unsigned>(received_size),
                     static_cast<unsigned>(bytes_read));
          }
        }
      } else if (packet.err != DMX_OK) {
        ESP_LOGW(TAG, "DMX error: %d", packet.err);
      }
    }
  }

  if (!want_tx)
    return;

  // Half-duplex: do not TX in the same loop as RX, and wait after last RX.
  if (this->mode_ == DMXMode::BIDIRECTIONAL) {
    if (got_rx)
      return;
    if ((millis() - this->last_rx_ms_) < this->rx_guard_ms_)
      return;
  }

  const uint32_t now = millis();
  if (this->tx_dirty_ || (now - this->last_tx_ms_ >= this->tx_interval_ms_)) {
    this->send_universe_();
    this->tx_dirty_ = false;
    this->last_tx_ms_ = now;
  }
}

void DMXComponent::publish_inputs_() {
