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

void RFBridgeComponent::decode_() {
  uint8_t action = uartbuf_[0];
  RFBridgeData data{};

  switch (action) {
    case RF_CODE_ACK:
      ESP_LOGD(TAG, "Action OK");
      break;
    case RF_CODE_LEARN_KO:
      this->ack_();
      ESP_LOGD(TAG, "Learn timeout");
      break;
    case RF_CODE_LEARN_OK:
      ESP_LOGD(TAG, "Learn started");
    case RF_CODE_RFIN:
      this->ack_();

      data.sync = (uartbuf_[1] << 8) | uartbuf_[2];
      data.low = (uartbuf_[3] << 8) | uartbuf_[4];
      data.high = (uartbuf_[5] << 8) | uartbuf_[6];
      data.code = (uartbuf_[7] << 16) | (uartbuf_[8] << 8) | uartbuf_[9];

      ESP_LOGD(TAG, "Received RFBridge Code: sync=0x%04X low=0x%04X high=0x%04X code=0x%06X", data.sync, data.low,
               data.high, data.code);
      this->callback_.call(data);
      break;
    default:
      ESP_LOGD(TAG, "Unknown action: 0x%02X", action);
      break;
  }
  this->last_ = millis();
}

void RFBridgeComponent::loop() {
  bool receiving = false;
  if (this->last_ != 0 && millis() - this->last_ > RF_DEBOUNCE) {
    this->last_ = 0;
  }

  while (this->available()) {
    uint8_t c = this->read();
    if (receiving) {
      if (c == RF_CODE_STOP && (this->uartpos_ == 1 || this->uartpos_ == RF_MESSAGE_SIZE + 1)) {
        this->decode_();
        receiving = false;
      } else if (this->uartpos_ <= RF_MESSAGE_SIZE) {
        this->uartbuf_[uartpos_++] = c;
      } else {
        receiving = false;
      }
    } else if (c == RF_CODE_START) {
      this->uartpos_ = 0;
      receiving = true;
    }
  }
}

void RFBridgeComponent::send_code(RFBridgeData data) {
  ESP_LOGD(TAG, "Sending code: sync=0x%04X low=0x%04X high=0x%04X code=0x%06X", data.sync, data.low, data.high,
           data.code);
  this->write(RF_CODE_START);
  this->write(RF_CODE_RFOUT);
  this->write(data.sync);
  this->write(data.low);
  this->write(data.high);
  this->write(data.code);
  this->write(RF_CODE_STOP);
}

void RFBridgeComponent::learn() {
  ESP_LOGD(TAG, "Learning mode");
  this->write(RF_CODE_START);
  this->write(RF_CODE_LEARN);
  this->write(RF_CODE_STOP);
}

void RFBridgeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RF_Bridge:");
  this->check_uart_settings(19200);
}

}  // namespace rf_bridge
}  // namespace esphome
