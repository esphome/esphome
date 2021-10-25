#include "modbus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace modbus {

static const char *const TAG = "modbus";

void Modbus::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }
}
void Modbus::loop() {
  const uint32_t now = millis();

  if (now - this->last_modbus_byte_ > 50) {
    this->rx_buffer_.clear();
    this->last_modbus_byte_ = now;
  }
  // stop blocking new send commands after send_wait_time_ ms regardless if a response has been received since then
  if (now - this->last_send_ > send_wait_time_) {
    waiting_for_response = 0;
  }

  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    if (this->parse_modbus_byte_(byte)) {
      this->last_modbus_byte_ = now;
    } else {
      this->rx_buffer_.clear();
    }
  }
}

uint16_t crc16(const uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

bool Modbus::parse_modbus_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  const uint8_t *raw = &this->rx_buffer_[0];
  ESP_LOGV(TAG, "Modbus received Byte  %d (0X%x)", byte, byte);
  // Byte 0: modbus address (match all)
  if (at == 0)
    return true;
  uint8_t address = raw[0];
  uint8_t function_code = raw[1];
  // Byte 2: Size (with modbus rtu function code 4/3)
  // See also https://en.wikipedia.org/wiki/Modbus
  if (at == 2)
    return true;

  uint8_t data_len = raw[2];
  uint8_t data_offset = 3;
  // the response for write command mirrors the requests and data startes at offset 2 instead of 3 for read commands
  if (function_code == 0x5 || function_code == 0x06 || function_code == 0x10) {
    data_offset = 2;
    data_len = 4;
  }

  // Error ( msb indicates error )
  // response format:  Byte[0] = device address, Byte[1] function code | 0x80 , Byte[2] excpetion code, Byte[3-4] crc
  if ((function_code & 0x80) == 0x80) {
    data_offset = 2;
    data_len = 1;
  }

  // Byte data_offset..data_offset+data_len-1: Data
  if (at < data_offset + data_len)
    return true;

  // Byte 3+data_len: CRC_LO (over all bytes)
  if (at == data_offset + data_len)
    return true;

  // Byte data_offset+len+1: CRC_HI (over all bytes)
  uint16_t computed_crc = crc16(raw, data_offset + data_len);
  uint16_t remote_crc = uint16_t(raw[data_offset + data_len]) | (uint16_t(raw[data_offset + data_len + 1]) << 8);
  if (computed_crc != remote_crc) {
    ESP_LOGW(TAG, "Modbus CRC Check failed! %02X!=%02X", computed_crc, remote_crc);
    return false;
  }
  std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + data_offset + data_len);
  bool found = false;
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      // Is it an error response?
      if ((function_code & 0x80) == 0x80) {
        ESP_LOGD(TAG, "Modbus error function code: 0x%X exception: %d", function_code, raw[2]);
        if (waiting_for_response != 0) {
          device->on_modbus_error(function_code & 0x7F, raw[2]);
        } else {
          // Ignore modbus exception not related to a pending command
          ESP_LOGD(TAG, "Ignoring Modbus error - not expecting a response");
        }
      } else {
        device->on_modbus_data(data);
      }
      found = true;
    }
  }
  waiting_for_response = 0;

  if (!found) {
    ESP_LOGW(TAG, "Got Modbus frame from unknown address 0x%02X! ", address);
  }

  // return false to reset buffer
  return false;
}

void Modbus::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  ESP_LOGCONFIG(TAG, "  Send Wait Time: %d ms", this->send_wait_time_);
}
float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void Modbus::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                  uint8_t payload_len, const uint8_t *payload) {
  static const size_t MAX_VALUES = 128;

  // Only check max number of registers for standard function codes
  // Some devices use non standard codes like 0x43
  if (number_of_entities > MAX_VALUES && function_code <= 0x10) {
    ESP_LOGE(TAG, "send too many values %d max=%zu", number_of_entities, MAX_VALUES);
    return;
  }

  std::vector<uint8_t> data;
  data.push_back(address);
  data.push_back(function_code);
  data.push_back(start_address >> 8);
  data.push_back(start_address >> 0);
  if (function_code != 0x5 && function_code != 0x6) {
    data.push_back(number_of_entities >> 8);
    data.push_back(number_of_entities >> 0);
  }

  if (payload != nullptr) {
    if (function_code == 0xF || function_code == 0x10) {  // Write multiple
      data.push_back(payload_len);                        // Byte count is required for write
    } else {
      payload_len = 2;  // Write single register or coil
    }
    for (int i = 0; i < payload_len; i++) {
      data.push_back(payload[i]);
    }
  }

  auto crc = crc16(data.data(), data.size());
  data.push_back(crc >> 0);
  data.push_back(crc >> 8);

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  this->write_array(data);
  this->flush();

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);
  waiting_for_response = address;
  last_send_ = millis();
  ESP_LOGV(TAG, "Modbus write: %s", hexencode(data).c_str());
}

// Helper function for lambdas
// Send raw command. Except CRC everything must be contained in payload
void Modbus::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  auto crc = crc16(payload.data(), payload.size());
  this->write_array(payload);
  this->write_byte(crc & 0xFF);
  this->write_byte((crc >> 8) & 0xFF);
  this->flush();
  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);
  waiting_for_response = payload[0];
  ESP_LOGV(TAG, "Modbus write raw: %s", hexencode(payload).c_str());
  last_send_ = millis();
}

}  // namespace modbus
}  // namespace esphome
