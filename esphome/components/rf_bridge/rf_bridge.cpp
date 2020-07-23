#include "rf_bridge.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace rf_bridge {

static const char *TAG = "rf_bridge";

void RFBridgeComponent::ack_() {
  ESP_LOGV(TAG, "Sending ACK");
  this->write(RF_CODE_START);
  this->write(RF_CODE_ACK);
  this->write(RF_CODE_STOP);
  this->flush();
}

bool RFBridgeComponent::parse_bridge_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  const uint8_t *raw = &this->rx_buffer_[0];

  // Byte 0: Start
  if (at == 0)
    return byte == RF_CODE_START;

  // Byte 1: Action
  if (at == 1)
    return byte >= RF_CODE_ACK && byte <= RF_CODE_RFOUT;
  uint8_t action = raw[1];

  switch (action) {
    case RF_CODE_ACK:
      ESP_LOGD(TAG, "Action OK");
      break;
    case RF_CODE_LEARN_KO:
      ESP_LOGD(TAG, "Learning timeout");
      break;
    case RF_CODE_LEARN_OK:
    case RF_CODE_RFIN:
      if (at < RF_MESSAGE_SIZE + 2)
        return true;

      RFBridgeData data;
      data.sync = (raw[2] << 8) | raw[3];
      data.low = (raw[4] << 8) | raw[5];
      data.high = (raw[6] << 8) | raw[7];
      data.code = (raw[8] << 16) | (raw[9] << 8) | raw[10];

      if (action == RF_CODE_LEARN_OK)
        ESP_LOGD(TAG, "Learning success");

      ESP_LOGD(TAG, "Received RFBridge Code: sync=0x%04X low=0x%04X high=0x%04X code=0x%06X", data.sync, data.low,
               data.high, data.code);
      this->callback_.call(data);
      break;
    default:
      ESP_LOGW(TAG, "Unknown action: 0x%02X", action);
      break;
  }

  if (byte == RF_CODE_STOP && action != RF_CODE_ACK)
    this->ack_();

  // return false to reset buffer
  return false;
}

void RFBridgeComponent::loop() {
  const uint32_t now = millis();
  if (now - this->last_bridge_byte_ > 50) {
    this->rx_buffer_.clear();
    this->last_bridge_byte_ = now;
  }

  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    if (this->parse_bridge_byte_(byte)) {
      this->last_bridge_byte_ = now;
    } else {
      this->rx_buffer_.clear();
    }
  }
}

void RFBridgeComponent::send_code(RFBridgeData data) {
  ESP_LOGD(TAG, "Sending code: sync=0x%04X low=0x%04X high=0x%04X code=0x%06X", data.sync, data.low, data.high,
           data.code);
  this->write(RF_CODE_START);
  this->write(RF_CODE_RFOUT);
  this->write((data.sync >> 8) & 0xFF);
  this->write(data.sync & 0xFF);
  this->write((data.low >> 8) & 0xFF);
  this->write(data.low & 0xFF);
  this->write((data.high >> 8) & 0xFF);
  this->write(data.high & 0xFF);
  this->write((data.code >> 16) & 0xFF);
  this->write((data.code >> 8) & 0xFF);
  this->write(data.code & 0xFF);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::learn() {
  ESP_LOGD(TAG, "Learning mode");
  this->write(RF_CODE_START);
  this->write(RF_CODE_LEARN);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RF_Bridge:");
  this->check_uart_settings(19200);
}

}  // namespace rf_bridge
}  // namespace esphome
