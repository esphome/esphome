#include "dmx_reader.h"
#include "esphome/core/log.h"

namespace esphome::dmx_reader {

ESPHOME_LOG_TAG(TAG, "dmx_reader");
static const char *const TAG = "dmx_reader";

void DMXReader::setup() {
  this->dmx_port_ = static_cast<dmx_port_t>(this->uart_num_);

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {{1, "ESPHome DMX Reader"}};
  dmx_driver_install(this->dmx_port_, &config, personalities, 1);

  int tx = this->tx_pin_->get_pin();
  int rx = this->rx_pin_->get_pin();
  int rts = this->rts_pin_ ? this->rts_pin_->get_pin() : -1;

  dmx_set_pin(this->dmx_port_, tx, rx, rts);

  ESP_LOGI(TAG, "DMX Reader started at UART%d (TX=%d RX=%d RTS=%d)", this->uart_num_, tx, rx, rts);
}

void DMXReader::loop() {
  dmx_packet_t packet;
  int size = dmx_receive(this->dmx_port_, &packet, 0);  // non-blocking

  if (size <= 0)
    return;

  if (packet.err != DMX_OK) {
    ESP_LOGW(TAG, "DMX error: %d", packet.err);
    return;
  }

  if (packet.is_rdm)
    return;

  dmx_read(this->dmx_port_, this->data_, size);

  // fixed channels
  for (auto &cs : this->static_sensors_) {
    if (cs.channel >= 1 && cs.channel <= 512) {
      float value = static_cast<float>(this->data_[cs.channel]);
      if (!cs.sensor->has_state() || cs.sensor->state != value) {
        cs.sensor->publish_state(value);
      }
    }
  }

  // Dynamic channels (via Number)
  for (auto &ds : this->dynamic_sensors_) {
    if (ds.channel_number == nullptr || !ds.channel_number->has_state())
      continue;

    uint16_t channel = static_cast<uint16_t>(ds.channel_number->state);
    if (channel < 1 || channel > 512)
      continue;

    float value = static_cast<float>(this->data_[channel]);
    if (!ds.sensor->has_state() || ds.sensor->state != value) {
      ds.sensor->publish_state(value);
    }
  }
}

uint8_t DMXReader::get_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512)
    return 0;
  return this->data_[channel];
}

void DMXReader::register_channel_sensor(uint16_t channel, sensor::Sensor *sens) {
  this->static_sensors_.push_back({channel, sens});
}

void DMXReader::register_dynamic_channel_sensor(number::Number *channel_number, sensor::Sensor *sens) {
  this->dynamic_sensors_.push_back({channel_number, sens});
}

void DMXReader::dump_config() {
  ESP_LOGCONFIG(TAG, "DMX Reader:");
  ESP_LOGCONFIG(TAG, "  UART: %d", this->uart_num_);
  LOG_PIN("  TX Pin: ", this->tx_pin_);
  LOG_PIN("  RX Pin: ", this->rx_pin_);
  if (this->rts_pin_) {
    LOG_PIN("  RTS Pin: ", this->rts_pin_);
  }
}

}  // namespace esphome::dmx_reader
