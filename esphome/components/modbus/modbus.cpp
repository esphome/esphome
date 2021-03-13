#include "modbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus {

static const char *TAG = "modbus";

void Modbus::loop() {
  const uint32_t now = millis();
  if (now - this->last_modbus_byte_ > 50) {
    this->rx_buffer_.clear();
    this->last_modbus_byte_ = now;
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
  ESP_LOGV(TAG, "Modbus recieved Byte  %d (0X%x)", byte, byte);
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
        ESP_LOGW(TAG, "Modbus error function code: 0x%X exception: %d", function_code, raw[2]);
        device->on_modbus_error(function_code & 0x7F, raw[2]);
      } else {
        device->on_modbus_data(data);
      }
      found = true;
    }
  }
  if (!found) {
    ESP_LOGW(TAG, "Got Modbus frame from unknown address 0x%02X!", address);
  }

  // return false to reset buffer
  return false;
}

void Modbus::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus:");
  if (ctrl_pin_)
    LOG_PIN("Ctrl Pin: ", ctrl_pin_);
}
float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

// Avoid copying the data and write the output directly
uint16_t update_crc16(uint16_t crc, uint8_t byte) {
  crc ^= byte;
  for (uint8_t i = 0; i < 8; i++) {
    if ((crc & 0x01) != 0) {
      crc >>= 1;
      crc ^= 0xA001;
    } else {
      crc >>= 1;
    }
  }
  return crc;
}

void Modbus::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t register_count,
                  const uint16_t *payload) {
  static const size_t MAX_VALUES = 128;
  if (ctrl_pin_) {
    ctrl_pin_->digital_write(TX_ENABLE);
  }

  if (register_count > MAX_VALUES) {
    ESP_LOGE(TAG, "send too many values %d max=%zu", register_count, MAX_VALUES);
    return;
  }
  uint16_t crc = 0xFFFF;
  this->write_byte(address);
  crc = update_crc16(crc, address);
  this->write_byte(function_code);
  crc = update_crc16(crc, function_code);
  this->write_byte(start_address >> 8);
  crc = update_crc16(crc, start_address >> 8);
  this->write_byte(start_address >> 0);
  crc = update_crc16(crc, start_address >> 0);
  this->write_byte(register_count >> 8);
  crc = update_crc16(crc, register_count >> 8);
  this->write_byte(register_count >> 0);
  crc = update_crc16(crc, register_count >> 0);
  uint8_t data_bytes = 6;

  // if this is a write command add the payload
  if (payload != nullptr) {
    this->write_byte(register_count * 2);  // Byte count is required for write
    crc = update_crc16(crc, register_count * 2);
    data_bytes++;
    for (int i = 0; i < register_count; i++) {
      this->write_byte(payload[i] >> 8);
      crc = update_crc16(crc, payload[i] >> 8);
      this->write_byte(payload[i] >> 0);
      crc = update_crc16(crc, payload[i] >> 0);
    }
  }
  this->write_byte(crc >> 0);
  this->write_byte(crc >> 8);
  this->flush();

  if (ctrl_pin_)
    ctrl_pin_->digital_write(RX_ENABLE);
}

}  // namespace modbus
}  // namespace esphome