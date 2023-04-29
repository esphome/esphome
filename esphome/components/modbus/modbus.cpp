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

bool Modbus::parse_modbus_byte_(uint8_t byte) {
  this->rx_buffer_.push_back(byte);
  size_t at = this->rx_buffer_.size();
  const uint8_t *raw = &this->rx_buffer_[0];
  ESP_LOGV(TAG, "Modbus received Byte  %d (0X%x)", byte, byte);
  // Byte 0: modbus address (match all)
  if (at < 2)
    return true;
  uint8_t address = raw[0];

  bool found = false;
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      // Byte 2: Size (with modbus rtu function code 4/3)
      // See also https://en.wikipedia.org/wiki/Modbus
      if (at < 3)
      	return true;
      auto data_info = device->get_message_info(raw, at);
      uint16_t data_len = data_info.first;
      uint16_t data_offset = data_info.second;

      // Byte data_offset..data_offset+data_len-1: Data
      if (at < data_offset + data_len)
        return true;
   
      // Byte 3+data_len: CRC_LO (over all bytes)
      /*if (at == data_offset + data_len + 1)
        return true;*/
   
      //ESP_LOGD(TAG, "CRC16 %i + %i = %i", data_offset, data_len, data_offset + data_len);
      //ESP_LOGD(TAG, "CRC offset: %u, len: %u, at: %u, data: %s %u", data_offset, data_len, at, format_hex_pretty(this->rx_buffer_).c_str(), data_offset + data_len); 
      // Byte data_offset+len+1: CRC_HI (over all bytes)
      uint16_t computed_crc = crc16(raw, data_offset + data_len);
      // Byte data_offset..data_offset+data_len-1: Data
      if (at < data_offset + data_len + 2) {
        return true;
      }

     //ESP_LOGD(TAG, "CRC offset: %u, len: %u, at: %u, data: %s %u", data_offset, data_len, at, format_hex_pretty(this->rx_buffer_).c_str(), data_offset + data_len + 2);
     uint16_t remote_crc = uint16_t(raw[data_offset + data_len]) | (uint16_t(raw[data_offset + data_len + 1]) << 8);
     if (computed_crc != remote_crc) {
       if (this->disable_crc_) {
         ESP_LOGD(TAG, "Modbus CRC Check failed, but ignored! %02X!=%02X", computed_crc, remote_crc);
       } else {
         ESP_LOGW(TAG, "Modbus CRC Check failed! %02X!=%02X", computed_crc, remote_crc);
         return false;
       }
     }
     std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + data_offset + data_len);
     uint8_t function_code = raw[1];
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
        ESP_LOGD(TAG, "Data %u+%u len: %u", data_offset, data_len, data.size());
        device->on_modbus_data(data);
      }
      found = true;
    }
  }
  waiting_for_response = 0;

  if (!found) {
    ESP_LOGD(TAG, "Got Skipping byte 0x%02X because the is not an address we know! ", address);
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + 1);
  }

  // return false to reset buffer
  return false;
}

void Modbus::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  ESP_LOGCONFIG(TAG, "  Send Wait Time: %d ms", this->send_wait_time_);
  ESP_LOGCONFIG(TAG, "  CRC Disabled: %s", YESNO(this->disable_crc_));
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
  ESP_LOGV(TAG, "Modbus write: %s", format_hex_pretty(data).c_str());
}
void Modbus::send_response(uint8_t address, uint8_t function_code, uint16_t start_address, uint8_t payload_len, const uint8_t *payload) {
  ESP_LOGD(TAG, "Modbus::send_response %02x %02x len %i", address, function_code, payload_len);
  std::vector<uint8_t> data;
  data.push_back(address);
  data.push_back(function_code);
  //data.push_back(start_address >> 8);
  //data.push_back(start_address >> 0);
  if (payload != nullptr) {
    data.push_back(payload_len);                        // Byte count is required for write
    for (int i = 0; i < payload_len; i++) {
      data.push_back(payload[i]);
    }
  }
  auto crc = crc16(data.data(), data.size());
  data.push_back(crc >> 0);
  data.push_back(crc >> 8);
  ESP_LOGD(TAG, "repononse %s", format_hex_pretty(data).c_str());

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  this->write_array(data);
  this->flush();

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);
  last_send_ = millis();
  ESP_LOGV(TAG, "Modbus response write: %s", format_hex_pretty(data).c_str());
}

// Helper function for lambdas
// Send raw command. Except CRC everything must be contained in payload
void Modbus::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }

  auto crc = crc16(payload.data(), payload.size());
  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  this->write_array(payload);
  this->write_byte(crc & 0xFF);
  this->write_byte((crc >> 8) & 0xFF);
  this->flush();
  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);
  waiting_for_response = payload[0];
  ESP_LOGV(TAG, "Modbus write raw: %s", format_hex_pretty(payload).c_str());
  last_send_ = millis();
}

void number_to_payload(std::vector<uint16_t> &data, int64_t value, modbus::SensorValueType value_type) {
  switch (value_type) {
    case modbus::SensorValueType::U_WORD:
    case modbus::SensorValueType::S_WORD:
      data.push_back(value & 0xFFFF);
      break;
    case modbus::SensorValueType::U_DWORD:
    case modbus::SensorValueType::S_DWORD:
    case modbus::SensorValueType::FP32:
    case modbus::SensorValueType::FP32_R:
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case modbus::SensorValueType::U_DWORD_R:
    case modbus::SensorValueType::S_DWORD_R:
      data.push_back(value & 0xFFFF);
      data.push_back((value & 0xFFFF0000) >> 16);
      break;
    case modbus::SensorValueType::U_QWORD:
    case modbus::SensorValueType::S_QWORD:
      data.push_back((value & 0xFFFF000000000000) >> 48);
      data.push_back((value & 0xFFFF00000000) >> 32);
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case modbus::SensorValueType::U_QWORD_R:
    case modbus::SensorValueType::S_QWORD_R:
      data.push_back(value & 0xFFFF);
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back((value & 0xFFFF00000000) >> 32);
      data.push_back((value & 0xFFFF000000000000) >> 48);
      break;
    default:
      ESP_LOGE(TAG, "Invalid data type for modbus number to payload conversation: %d",
               static_cast<uint16_t>(value_type));
      break;
  }
}

int64_t payload_to_number(const std::vector<uint8_t> &data, modbus::SensorValueType sensor_value_type, uint8_t offset,
                          uint32_t bitmask) {
  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits

  switch (sensor_value_type) {
    case modbus::SensorValueType::U_WORD:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      break;
    case modbus::SensorValueType::U_DWORD:
    case modbus::SensorValueType::FP32:
      value = get_data<uint32_t>(data, offset);
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case modbus::SensorValueType::U_DWORD_R:
    case modbus::SensorValueType::FP32_R:
      value = get_data<uint32_t>(data, offset);
      value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case modbus::SensorValueType::S_WORD:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, offset),
                                         bitmask);  // default is 0xFFFF ;
      break;
    case modbus::SensorValueType::S_DWORD:
      value = mask_and_shift_by_rightbit(get_data<int32_t>(data, offset), bitmask);
      break;
    case modbus::SensorValueType::S_DWORD_R: {
      value = get_data<uint32_t>(data, offset);
      // Currently the high word is at the low position
      // the sign bit is therefore at low before the switch
      uint32_t sign_bit = (value & 0x8000) << 16;
      value = mask_and_shift_by_rightbit(
          static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), bitmask);
    } break;
    case modbus::SensorValueType::U_QWORD:
    case modbus::SensorValueType::S_QWORD:
      // Ignore bitmask for QWORD
      value = get_data<uint64_t>(data, offset);
      break;
    case modbus::SensorValueType::U_QWORD_R:
    case modbus::SensorValueType::S_QWORD_R: {
      // Ignore bitmask for QWORD
      uint64_t tmp = get_data<uint64_t>(data, offset);
      value = (tmp << 48) | (tmp >> 48) | ((tmp & 0xFFFF0000) << 16) | ((tmp >> 16) & 0xFFFF0000);
    } break;
    case modbus::SensorValueType::RAW:
    default:
      break;
  }
  return value;
}

}  // namespace modbus
}  // namespace esphome
