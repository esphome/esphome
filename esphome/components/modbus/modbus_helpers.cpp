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
                          uint32_t bitmask) {
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
  return value;
}
}  // namespace esphome::modbus::helpers
