#include "dmx.h"
#include "esphome/core/log.h"

namespace esphome::dmx {

ESPHOME_LOG_TAG(TAG, "dmx");

void DMXComponent::setup() {
  this->dmx_port_ = static_cast<dmx_port_t>(this->uart_num_);
  this->data_[0] = 0;  // null start code

  if (uart_is_driver_installed(this->dmx_port_)) {
    ESP_LOGE(TAG,
             "UART%d is already in use. "
             "Check logger and UART configuration.",
             this->uart_num_);
    this->mark_failed();
    return;
  }

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
  // --- RX ---
  dmx_packet_t packet;
  int size = static_cast<int>(dmx_receive(this->dmx_port_, &packet, 0));
  if (size > 0) {
    if (packet.err == DMX_OK && !packet.is_rdm && packet.sc == 0x00) {
      dmx_read(this->dmx_port_, this->data_, static_cast<size_t>(size));
      this->publish_inputs_();
    } else if (packet.err != DMX_OK) {
      ESP_LOGW(TAG, "DMX error: %d", packet.err);
    }
  }

  // --- TX (continuous refresh when outputs are used) ---
  if (this->tx_enabled_) {
    const uint32_t now = millis();
    if (this->tx_dirty_ || (now - this->last_tx_ms_ >= TX_INTERVAL_MS)) {
      this->send_universe_();
      this->tx_dirty_ = false;
      this->last_tx_ms_ = now;
    }
  }
}

void DMXComponent::publish_inputs_() {
  for (auto &cs : this->static_sensors_) {
    if (cs.channel < 1 || cs.channel > 512)
      continue;
    float value = static_cast<float>(this->data_[cs.channel]);
    if (!cs.sensor->has_state() || cs.sensor->state != value) {
      cs.sensor->publish_state(value);
    }
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
    float value = static_cast<float>(this->data_[channel]);
    if (!ds.sensor->has_state() || ds.sensor->state != value) {
      ds.sensor->publish_state(value);
    }
  }
}

void DMXComponent::send_universe_() {
  dmx_write(this->dmx_port_, this->data_, DMX_PACKET_SIZE);
  dmx_send_num(this->dmx_port_, DMX_PACKET_SIZE);
}

uint8_t DMXComponent::get_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512)
    return 0;
  return this->data_[channel];
}

void DMXComponent::set_slot(uint16_t channel, uint8_t value) {
  if (channel < 1 || channel > 512)
    return;
  if (this->data_[channel] == value)
    return;
  this->data_[channel] = value;
  this->tx_dirty_ = true;
}

void DMXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DMX Hub:");
  ESP_LOGCONFIG(TAG, "  UART: %d", this->uart_num_);
  ESP_LOGCONFIG(TAG, "  TX Pin: GPIO%d", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  RX Pin: GPIO%d", this->rx_pin_);
  if (this->rts_pin_ >= 0) {
    ESP_LOGCONFIG(TAG, "  RTS Pin: GPIO%d", this->rts_pin_);
  }
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
