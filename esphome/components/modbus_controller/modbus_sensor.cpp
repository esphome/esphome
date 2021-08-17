
#include "modbus_sensor.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_sensor";

// ModbusSensor
// Extract bits from value and shift right according to the bitmask
// if the bitmask is 0x00F0  we want the values frrom bit 5 - 8.
// the result is then shifted right by the postion if the first right set bit in the mask
// Usefull for modbus data where more than one value is packed in a 16 bit register
// Example: on Epever the "Length of night" register 0x9065 encodes values of the whole night length of time as
// D15 - D8 =  hour, D7 - D0 = minute
// To get the hours use mask 0xFF00 and  0x00FF for the minute
template<typename N> N mask_and_shift_by_rightbit(N data, uint32_t mask) {
  auto result = (mask & data);
  if (result == 0) {
    return result;
  }
  for (int pos = 0; pos < sizeof(N) << 3; pos++) {
    if ((mask & (1 << pos)) != 0)
      return result >> pos;
  }
  return 0;
}

void ModbusSensor::log() { LOG_SENSOR(TAG, get_name().c_str(), this); }

void ModbusSensor::add_to_controller(ModbusController *master, ModbusFunctionCode register_type, uint16_t start_address,
                                     uint8_t offset, uint32_t bitmask, SensorValueType value_type, int register_count,
                                     uint8_t skip_updates) {
  this->register_type = register_type;
  this->start_address = start_address;
  this->offset = offset;
  this->bitmask = bitmask;
  this->sensor_value_type = value_type;
  this->register_count = register_count;
  this->skip_updates = 0;
  this->parent_ = master;
  master->add_sensor_item(this);
}

float ModbusSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  union {
    float float_value;
    uint32_t raw;
  } raw_to_float;

  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits
  float result = NAN;

  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, this->offset), this->bitmask);  // default is 0xFFFF ;
      result = static_cast<float>(value);
      break;
    case SensorValueType::U_DWORD:
      value = get_data<uint32_t>(data, this->offset);
      value = mask_and_shift_by_rightbit((uint32_t) value, this->bitmask);
      result = static_cast<float>(value);
      break;
    case SensorValueType::U_DWORD_R:
      value = get_data<uint32_t>(data, this->offset);
      value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
      value = mask_and_shift_by_rightbit((uint32_t) value, this->bitmask);
      result = static_cast<float>(value);
      break;
    case SensorValueType::S_WORD:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, this->offset),
                                         this->bitmask);  // default is 0xFFFF ;
      result = static_cast<float>(value);
      break;
    case SensorValueType::S_DWORD:
      value = mask_and_shift_by_rightbit(get_data<int32_t>(data, this->offset), this->bitmask);
      break;
    case SensorValueType::S_DWORD_R: {
      value = get_data<uint32_t>(data, this->offset);
      // Currently the high word is at the low position
      // the sign bit is therefore at low before the switch
      uint32_t sign_bit = (value & 0x8000) << 16;
      value = mask_and_shift_by_rightbit(
          static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), this->bitmask);
      result = static_cast<float>(value);
    } break;
    case SensorValueType::U_QWORD:
      // Ignore bitmask for U_QWORD
      value = get_data<uint64_t>(data, this->offset);
      result = static_cast<float>(value);
      break;

    case SensorValueType::S_QWORD:
      // Ignore bitmask for S_QWORD
      value = get_data<int64_t>(data, this->offset);
      result = static_cast<float>(value);
      break;
    case SensorValueType::U_QWORD_R:
      // Ignore bitmask for U_QWORD
      value = get_data<uint64_t>(data, this->offset);
      value = static_cast<uint64_t>(value & 0xFFFF) << 48 | (value & 0xFFFF000000000000) >> 48 |
              static_cast<uint64_t>(value & 0xFFFF0000) << 32 | (value & 0x0000FFFF00000000) >> 32 |
              static_cast<uint64_t>(value & 0xFFFF00000000) << 16 | (value & 0x00000000FFFF0000) >> 16;
      result = static_cast<float>(value);
      break;

    case SensorValueType::S_QWORD_R:
      // Ignore bitmask for S_QWORD
      value = get_data<int64_t>(data, this->offset);
      result = static_cast<float>(value);
      break;
    case SensorValueType::FP32:
      raw_to_float.raw = get_data<uint32_t>(data, this->offset);
      ESP_LOGD(TAG, "FP32 = 0x%08X => %f", raw_to_float.raw, raw_to_float.float_value);
      result = raw_to_float.float_value;
      // Testing only show FP32_R value as well
      {
        auto tmp = get_data<uint32_t>(data, this->offset);
        raw_to_float.raw = static_cast<uint32_t>(tmp & 0xFFFF) << 16 | (tmp & 0xFFFF0000) >> 16;
        ESP_LOGD(TAG, "FP32_R = 0x%08X => %f", raw_to_float.raw, raw_to_float.float_value);
      }
      break;
    case SensorValueType::FP32_R: {
      auto tmp = get_data<uint32_t>(data, this->offset);
      raw_to_float.raw = static_cast<uint32_t>(tmp & 0xFFFF) << 16 | (tmp & 0xFFFF0000) >> 16;
      ESP_LOGD(TAG, "FP32_R = 0x%08X => %f", raw_to_float.raw, raw_to_float.float_value);
      result = raw_to_float.float_value;
      // testing only 
      raw_to_float.raw = tmp ; 
      ESP_LOGD(TAG, "FP32 = 0x%08X => %f", raw_to_float.raw, raw_to_float.float_value);      
    } break;
    default:
      break;
  }

  // No need to publish if the value didn't change since the last publish
  // can reduce mqtt traffic considerably if many sensors are used
  ESP_LOGVV(TAG, " SENSOR : new: %lld", value);
  // this->sensor_->raw_state = result;
  this->publish_state(result);
  return result;
}

// End ModbusSensor

}  // namespace modbus_controller
}  // namespace esphome
