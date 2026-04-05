#include "modbus_helpers.h"
#include "esphome/core/log.h"

namespace esphome::modbus::helpers {

static const char *const TAG = "modbus_helpers";

void number_to_payload(std::vector<uint16_t> &data, int64_t value, SensorValueType value_type) {
  switch (value_type) {
    case SensorValueType::U_WORD:
    case SensorValueType::S_WORD:
      data.push_back(value & 0xFFFF);
      break;
    case SensorValueType::U_DWORD:
    case SensorValueType::S_DWORD:
    case SensorValueType::FP32:
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::S_DWORD_R:
    case SensorValueType::FP32_R:
      data.push_back(value & 0xFFFF);
      data.push_back((value & 0xFFFF0000) >> 16);
      break;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
      data.push_back((value & 0xFFFF000000000000) >> 48);
      data.push_back((value & 0xFFFF00000000) >> 32);
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R:
      data.push_back(value & 0xFFFF);
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back((value & 0xFFFF00000000) >> 32);
      data.push_back((value & 0xFFFF000000000000) >> 48);
      break;
    default:
      ESP_LOGE(TAG, "Invalid data type for modbus number to payload conversion: %d", static_cast<uint16_t>(value_type));
      break;
  }
}

int64_t payload_to_number(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                          uint32_t bitmask, bool *error_return) {
  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits

  if (offset > data.size()) {
    ESP_LOGE(TAG, "not enough data for value");
    return value;
  }

  size_t size = data.size() - offset;
  bool error = false;
  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      if (size >= 2) {
        value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, offset),
                                           bitmask);  // default is 0xFFFF ;
      } else {
        error = true;
      }
      break;
    case SensorValueType::U_DWORD:
    case SensorValueType::FP32:
      if (size >= 4) {
        value = get_data<uint32_t>(data, offset);
        value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      } else {
        error = true;
      }
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::FP32_R:
      if (size >= 4) {
        value = get_data<uint32_t>(data, offset);
        value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
        value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      } else {
        error = true;
      }
      break;
    case SensorValueType::S_WORD:
      if (size >= 2) {
        value = mask_and_shift_by_rightbit(get_data<int16_t>(data, offset),
                                           bitmask);  // default is 0xFFFF ;
      } else {
        error = true;
      }
      break;
    case SensorValueType::S_DWORD:
      if (size >= 4) {
        value = mask_and_shift_by_rightbit(get_data<int32_t>(data, offset), bitmask);
      } else {
        error = true;
      }
      break;
    case SensorValueType::S_DWORD_R: {
      if (size >= 4) {
        value = get_data<uint32_t>(data, offset);
        // Currently the high word is at the low position
        // the sign bit is therefore at low before the switch
        uint32_t sign_bit = (value & 0x8000) << 16;
        value = mask_and_shift_by_rightbit(
            static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), bitmask);
      } else {
        error = true;
      }
    } break;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
      // Ignore bitmask for QWORD
      if (size >= 8) {
        value = get_data<uint64_t>(data, offset);
      } else {
        error = true;
      }
      break;
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R: {
      // Ignore bitmask for QWORD
      if (size >= 8) {
        uint64_t tmp = get_data<uint64_t>(data, offset);
        value = (tmp << 48) | (tmp >> 48) | ((tmp & 0xFFFF0000) << 16) | ((tmp >> 16) & 0xFFFF0000);
      } else {
        error = true;
      }
    } break;
    case SensorValueType::RAW:
    default:
      break;
  }
  if (error)
    ESP_LOGE(TAG, "not enough data for value");
  if (error_return)
    *error_return = error;
  return value;
}

StaticVector<uint8_t, MAX_FRAME_SIZE> create_client_pdu(ModbusFunctionCode function_code, uint16_t start_address,
                                                        uint16_t number_of_entities, const uint8_t *values,
                                                        size_t values_len) {
  StaticVector<uint8_t, MAX_FRAME_SIZE> pdu;
  pdu.push_back(static_cast<uint8_t>(function_code));
  pdu.push_back(start_address >> 8);
  pdu.push_back(start_address >> 0);
  if (function_code != ModbusFunctionCode::WRITE_SINGLE_COIL &&
      function_code != ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
    pdu.push_back(number_of_entities >> 8);
    pdu.push_back(number_of_entities >> 0);
  }
  if (values_len > 0 && is_function_code_write(static_cast<uint8_t>(function_code))) {
    if (function_code == ModbusFunctionCode::WRITE_MULTIPLE_COILS ||
        function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
      pdu.push_back(values_len);  // Byte count is required for write multiple
      for (size_t i = 0; i < values_len; i++)
        pdu.push_back(values[i]);
    } else {
      // Write single register or coil (2 bytes)
      pdu.push_back(values[0]);
      pdu.push_back(values[1]);
    }
  }
  return pdu;
}

StaticVector<uint8_t, MAX_FRAME_SIZE> create_write_multiple_pdu(uint16_t start_address, uint16_t register_count,
                                                                const std::vector<uint16_t> &values) {
  uint8_t payload[MAX_FRAME_SIZE];
  size_t payload_len = 0;
  for (auto v : values) {
    auto decoded_value = decode_value(v);
    payload[payload_len++] = decoded_value[0];
    payload[payload_len++] = decoded_value[1];
  }
  return create_client_pdu(ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS, start_address, register_count, payload,
                           payload_len);
}

StaticVector<uint8_t, MAX_FRAME_SIZE> create_write_single_pdu(uint16_t start_address, uint16_t value) {
  auto decoded_value = decode_value(value);
  uint8_t payload[2] = {decoded_value[0], decoded_value[1]};
  return create_client_pdu(ModbusFunctionCode::WRITE_SINGLE_REGISTER, start_address, 1, payload, sizeof(payload));
}

StaticVector<uint8_t, MAX_FRAME_SIZE> create_write_single_coil_pdu(uint16_t address, bool value) {
  uint8_t payload[2] = {uint8_t(value ? 0xFF : 0), 0};
  return create_client_pdu(ModbusFunctionCode::WRITE_SINGLE_COIL, address, 1, payload, sizeof(payload));
}

StaticVector<uint8_t, MAX_FRAME_SIZE> create_write_multiple_coils_pdu(uint16_t start_address,
                                                                      const std::vector<bool> &values) {
  uint8_t payload[MAX_FRAME_SIZE];
  size_t payload_len = 0;
  uint8_t bitmask = 0;
  int bitcounter = 0;
  for (auto coil : values) {
    if (coil) {
      bitmask |= (1 << bitcounter);
    }
    bitcounter++;
    if (bitcounter % 8 == 0) {
      payload[payload_len++] = bitmask;
      bitmask = 0;
    }
  }
  // add remaining bits
  if (bitcounter % 8) {
    payload[payload_len++] = bitmask;
  }
  return create_client_pdu(ModbusFunctionCode::WRITE_MULTIPLE_COILS, start_address, values.size(), payload,
                           payload_len);
}

std::vector<uint8_t> add_crc_to_payload(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> data = payload;
  auto crc = crc16(data.data(), data.size());
  data.push_back(crc >> 0);
  data.push_back(crc >> 8);
  return data;
}

}  // namespace esphome::modbus::helpers
