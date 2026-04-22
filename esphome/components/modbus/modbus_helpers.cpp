#include "modbus_helpers.h"
#include "esphome/core/log.h"

namespace esphome::modbus::helpers {

static const char *const TAG = "modbus_helpers";

static size_t required_payload_size(SensorValueType sensor_value_type) {
  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
    case SensorValueType::S_WORD:
      return 2;
    case SensorValueType::U_DWORD:
    case SensorValueType::FP32:
    case SensorValueType::U_DWORD_R:
    case SensorValueType::FP32_R:
    case SensorValueType::S_DWORD:
    case SensorValueType::S_DWORD_R:
      return 4;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R:
      return 8;
    case SensorValueType::RAW:
    default:
      return 0;
  }
}

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

  // Validate offset against the buffer for all types, including RAW/unsupported, so
  // a malformed or misconfigured frame still produces an error log.
  if (static_cast<size_t>(offset) > data.size()) {
    ESP_LOGE(TAG, "not enough data for value type=%u offset=%u size=%zu", static_cast<unsigned int>(sensor_value_type),
             static_cast<unsigned int>(offset), data.size());
    if (error_return)
      *error_return = true;
    return value;
  }

  const size_t required_size = required_payload_size(sensor_value_type);
  if (required_size == 0) {
    return value;
  }

  if (data.size() - offset < required_size) {
    ESP_LOGE(TAG, "not enough data for value type=%u offset=%u size=%zu required=%zu",
             static_cast<unsigned int>(sensor_value_type), static_cast<unsigned int>(offset), data.size(),
             required_size);
    if (error_return)
      *error_return = true;
    return value;
  }

  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::U_DWORD:
    case SensorValueType::FP32:
      value = get_data<uint32_t>(data, offset);
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::FP32_R:
      value = get_data<uint32_t>(data, offset);
      value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case SensorValueType::S_WORD:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::S_DWORD:
      value = mask_and_shift_by_rightbit(get_data<int32_t>(data, offset), bitmask);
      break;
    case SensorValueType::S_DWORD_R: {
      value = get_data<uint32_t>(data, offset);
      // Currently the high word is at the low position
      // the sign bit is therefore at low before the switch
      uint32_t sign_bit = (value & 0x8000) << 16;
      value = mask_and_shift_by_rightbit(
          static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), bitmask);
    } break;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
      // Ignore bitmask for QWORD
      value = get_data<uint64_t>(data, offset);
      break;
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R: {
      // Ignore bitmask for QWORD
      uint64_t tmp = get_data<uint64_t>(data, offset);
      value = (tmp << 48) | (tmp >> 48) | ((tmp & 0xFFFF0000) << 16) | ((tmp >> 16) & 0xFFFF0000);
    } break;
    case SensorValueType::RAW:
    default:
      break;
  }
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
}  // namespace esphome::modbus::helpers
