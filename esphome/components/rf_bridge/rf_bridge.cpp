#include "rf_bridge.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cinttypes>
#include <cstring>

namespace esphome {
namespace rf_bridge {

static const char *const TAG = "rf_bridge";

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

  ESP_LOGVV(TAG, "Processing byte: 0x%02X", byte);

  // Byte 0: Start
  if (at == 0)
    return byte == RF_CODE_START;

  // Byte 1: Action
  if (at == 1)
    return byte >= RF_CODE_ACK && byte <= RF_CODE_RFIN_BUCKET;
  uint8_t action = raw[1];

  switch (action) {
    case RF_CODE_ACK:
      ESP_LOGD(TAG, "Action OK");
      break;
    case RF_CODE_LEARN_KO:
      ESP_LOGD(TAG, "Learning timeout");
      break;
    case RF_CODE_LEARN_OK:
    case RF_CODE_RFIN: {
      if (byte != RF_CODE_STOP || at < RF_MESSAGE_SIZE + 2)
        return true;

      RFBridgeData data;
      data.sync = (raw[2] << 8) | raw[3];
      data.low = (raw[4] << 8) | raw[5];
      data.high = (raw[6] << 8) | raw[7];
      data.code = (raw[8] << 16) | (raw[9] << 8) | raw[10];

      if (action == RF_CODE_LEARN_OK) {
        ESP_LOGD(TAG, "Learning success");
      }

      ESP_LOGI(TAG,
               "Received RFBridge Code: sync=0x%04" PRIX16 " low=0x%04" PRIX16 " high=0x%04" PRIX16
               " code=0x%06" PRIX32,
               data.sync, data.low, data.high, data.code);
      this->data_callback_.call(data);
      break;
    }
    case RF_CODE_LEARN_OK_NEW:
    case RF_CODE_ADVANCED_RFIN: {
      if (byte != RF_CODE_STOP) {
        return at < (raw[2] + 3);
      }

      RFBridgeAdvancedData data{};

      data.length = raw[2];
      data.protocol = raw[3];
      char next_byte[3];
      for (uint8_t i = 0; i < data.length - 1; i++) {
        sprintf(next_byte, "%02X", raw[4 + i]);
        data.code += next_byte;
      }

      ESP_LOGI(TAG, "Received RFBridge Advanced Code: length=0x%02X protocol=0x%02X code=0x%s", data.length,
               data.protocol, data.code.c_str());
      this->advanced_data_callback_.call(data);
      break;
    }
    case RF_CODE_RFIN_BUCKET: {
      if (byte != RF_CODE_STOP) {
        return true;
      }

      uint8_t buckets = raw[2] << 1;
      std::string str;
      char next_byte[3];

      for (uint32_t i = 0; i <= at; i++) {
        sprintf(next_byte, "%02X", raw[i]);
        str += next_byte;
        if ((i > 3) && buckets) {
          buckets--;
        }
        if ((i < 3) || (buckets % 2) || (i == at - 1)) {
          str += " ";
        }
      }
      ESP_LOGI(TAG, "Received RFBridge Bucket: %s", str.c_str());
      break;
    }
    default:
      ESP_LOGW(TAG, "Unknown action: 0x%02X", action);
      break;
  }

  ESP_LOGVV(TAG, "Parsed: 0x%02X", byte);

  if (byte == RF_CODE_STOP && action != RF_CODE_ACK)
    this->ack_();

  // return false to reset buffer
  return false;
}

void RFBridgeComponent::write_byte_str_(const std::string &codes) {
  uint8_t code;
  int size = codes.length();
  for (int i = 0; i < size; i += 2) {
    code = strtol(codes.substr(i, 2).c_str(), nullptr, 16);
    this->write(code);
  }
}

void RFBridgeComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_bridge_byte_ > 50) {
    this->rx_buffer_.clear();
    this->last_bridge_byte_ = now;
  }

  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    if (this->parse_bridge_byte_(byte)) {
      ESP_LOGVV(TAG, "Parsed: 0x%02X", byte);
      this->last_bridge_byte_ = now;
    } else {
      this->rx_buffer_.clear();
    }
  }
}

void RFBridgeComponent::send_code(RFBridgeData data) {
  ESP_LOGD(TAG, "Sending code: sync=0x%04" PRIX16 " low=0x%04" PRIX16 " high=0x%04" PRIX16 " code=0x%06" PRIX32,
           data.sync, data.low, data.high, data.code);
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

void RFBridgeComponent::send_advanced_code(const RFBridgeAdvancedData &data) {
  ESP_LOGD(TAG, "Sending advanced code: length=0x%02X protocol=0x%02X code=0x%s", data.length, data.protocol,
           data.code.c_str());
  this->write(RF_CODE_START);
  this->write(RF_CODE_RFOUT_NEW);
  this->write(data.length & 0xFF);
  this->write(data.protocol & 0xFF);
  this->write_byte_str_(data.code);
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

void RFBridgeComponent::start_advanced_sniffing() {
  ESP_LOGI(TAG, "Advanced Sniffing on");
  this->write(RF_CODE_START);
  this->write(RF_CODE_SNIFFING_ON);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::stop_advanced_sniffing() {
  ESP_LOGI(TAG, "Advanced Sniffing off");
  this->write(RF_CODE_START);
  this->write(RF_CODE_SNIFFING_OFF);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::start_bucket_sniffing() {
  ESP_LOGI(TAG, "Raw Bucket Sniffing on");
  this->write(RF_CODE_START);
  this->write(RF_CODE_RFIN_BUCKET);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::send_raw(const std::string &raw_code) {
  ESP_LOGD(TAG, "Sending Raw Code: %s", raw_code.c_str());

  this->write_byte_str_(raw_code);
  this->flush();
}

void RFBridgeComponent::beep(uint16_t ms) {
  ESP_LOGD(TAG, "Beeping for %hu ms", ms);

  this->write(RF_CODE_START);
  this->write(RF_CODE_BEEP);
  this->write((ms >> 8) & 0xFF);
  this->write(ms & 0xFF);
  this->write(RF_CODE_STOP);
  this->flush();
}

}  // namespace rf_bridge
}  // namespace esphome
