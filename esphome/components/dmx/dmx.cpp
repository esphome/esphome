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
  for (auto &cs : this->static_sensors_) {
    if (cs.channel < 1 || cs.channel > 512)
      continue;
    const float value = static_cast<float>(this->rx_data_[cs.channel]);
    if (!cs.sensor->has_state() || cs.sensor->state != value)
      cs.sensor->publish_state(value);
  }

  for (auto &ds : this->dynamic_sensors_) {
    if (ds.channel_number == nullptr || !ds.channel_number->has_state())
      continue;
    const float channel_value = ds.channel_number->state;
    if (channel_value < 1.0f || channel_value > 512.0f)
      continue;
    const uint16_t channel = static_cast<uint16_t>(channel_value);
    if (static_cast<float>(channel) != channel_value)
      continue;
    const float value = static_cast<float>(this->rx_data_[channel]);
    if (!ds.sensor->has_state() || ds.sensor->state != value)
      ds.sensor->publish_state(value);
  }
}

void DMXComponent::send_universe_() {
  dmx_write(this->dmx_port_, this->tx_data_, DMX_PACKET_SIZE);
  dmx_send_num(this->dmx_port_, DMX_PACKET_SIZE);
}

uint8_t DMXComponent::get_rx_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512)
    return 0;
  return this->rx_data_[channel];
}

uint8_t DMXComponent::get_tx_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512)
    return 0;
  return this->tx_data_[channel];
}

void DMXComponent::set_slot(uint16_t channel, uint8_t value) { this->set_slots(channel, &value, 1); }

void DMXComponent::set_slots(uint16_t first_channel, const uint8_t *values, size_t count) {
  if (values == nullptr || count == 0 || first_channel < 1 || first_channel > 512)
    return;

  // Avoid writing beyond channel 512. Invalid ranges are ignored as a whole.
  if (count > (513U - static_cast<size_t>(first_channel)))
    return;

  bool changed = false;
  for (size_t i = 0; i < count; i++) {
    const size_t index = static_cast<size_t>(first_channel) + i;
    if (this->tx_data_[index] != values[i]) {
      this->tx_data_[index] = values[i];
      changed = true;
    }
  }

  if (changed)
    this->tx_dirty_ = true;
}

void DMXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DMX Hub:");
  ESP_LOGCONFIG(TAG, "  UART: %d", this->uart_num_);
  ESP_LOGCONFIG(TAG, "  Mode: %u", static_cast<unsigned>(this->mode_));
  ESP_LOGCONFIG(TAG, "  TX Pin: GPIO%d", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  RX Pin: GPIO%d", this->rx_pin_);
  if (this->rts_pin_ >= 0) {
    ESP_LOGCONFIG(TAG, "  RTS Pin: GPIO%d", this->rts_pin_);
  }
  ESP_LOGCONFIG(TAG, "  TX interval: %u ms", static_cast<unsigned>(this->tx_interval_ms_));
  ESP_LOGCONFIG(TAG, "  RX guard: %u ms", static_cast<unsigned>(this->rx_guard_ms_));
}

void DMXOutputNumber::control(float value) {
  if (this->parent_ == nullptr)
    return;
  if (value < 0.0f)
    value = 0.0f;
  if (value > 255.0f)
    value = 255.0f;
  const uint8_t slot = static_cast<uint8_t>(value);
  this->parent_->set_slot(this->channel_, slot);
  this->publish_state(slot);
}

void DMXOutputNumber::dump_config() {
  ESP_LOGCONFIG(TAG, "DMX Output:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
}

}  // namespace esphome::dmx
