#pragma once

#include <string>
#include <vector>
#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/components/modbus/modbus_definitions.h"

namespace esphome::modbus::helpers {

enum class SensorValueType : uint8_t {
  RAW = 0x00,     // variable length
  U_WORD = 0x1,   // 1 Register unsigned
  U_DWORD = 0x2,  // 2 Registers unsigned
  S_WORD = 0x3,   // 1 Register signed
  S_DWORD = 0x4,  // 2 Registers signed
  BIT = 0x5,
  U_DWORD_R = 0x6,  // 2 Registers unsigned
  S_DWORD_R = 0x7,  // 2 Registers unsigned
  U_QWORD = 0x8,
  S_QWORD = 0x9,
  U_QWORD_R = 0xA,
  S_QWORD_R = 0xB,
  FP32 = 0xC,
  FP32_R = 0xD
};

inline bool value_type_is_float(SensorValueType v) {
  return v == SensorValueType::FP32 || v == SensorValueType::FP32_R;
}

inline ModbusFunctionCode modbus_register_read_function(ModbusRegisterType reg_type) {
  switch (reg_type) {
    case ModbusRegisterType::COIL:
      return ModbusFunctionCode::READ_COILS;
    case ModbusRegisterType::DISCRETE_INPUT:
      return ModbusFunctionCode::READ_DISCRETE_INPUTS;
    case ModbusRegisterType::HOLDING:
      return ModbusFunctionCode::READ_HOLDING_REGISTERS;
    case ModbusRegisterType::READ:
      return ModbusFunctionCode::READ_INPUT_REGISTERS;
    default:
      return ModbusFunctionCode::CUSTOM;
  }
}

inline ModbusFunctionCode modbus_register_write_function(ModbusRegisterType reg_type) {
  switch (reg_type) {
    case ModbusRegisterType::COIL:
      return ModbusFunctionCode::WRITE_SINGLE_COIL;
    case ModbusRegisterType::DISCRETE_INPUT:
      return ModbusFunctionCode::CUSTOM;
    case ModbusRegisterType::HOLDING:
      return ModbusFunctionCode::READ_WRITE_MULTIPLE_REGISTERS;
    case ModbusRegisterType::READ:
    default:
      return ModbusFunctionCode::CUSTOM;
  }
}

inline uint8_t c_to_hex(char c) { return (c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0'); }

/** Get a byte from a hex string
 *  byte_from_hex_str("1122", 1) returns uint_8 value 0x22 == 34
 *  byte_from_hex_str("1122", 0) returns 0x11
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return byte value
 */
inline uint8_t byte_from_hex_str(const std::string &value, uint8_t pos) {
  if (value.length() < pos * 2 + 2)
    return 0;
  return (c_to_hex(value[pos * 2]) << 4) | c_to_hex(value[pos * 2 + 1]);
}

/** Get a word from a hex string
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return word value
 */
inline uint16_t word_from_hex_str(const std::string &value, uint8_t pos) {
  return byte_from_hex_str(value, pos) << 8 | byte_from_hex_str(value, pos + 1);
}

/** Get a dword from a hex string
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return dword value
 */
inline uint32_t dword_from_hex_str(const std::string &value, uint8_t pos) {
  return word_from_hex_str(value, pos) << 16 | word_from_hex_str(value, pos + 2);
}

/** Get a qword from a hex string
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return qword value
 */
inline uint64_t qword_from_hex_str(const std::string &value, uint8_t pos) {
  return static_cast<uint64_t>(dword_from_hex_str(value, pos)) << 32 | dword_from_hex_str(value, pos + 4);
}

// Extract data from modbus response buffer
/** Extract data from modbus response buffer
 * @param T one of supported integer data types int_8,int_16,int_32,int_64
 * @param data modbus response buffer (uint8_t)
 * @param buffer_offset  offset in bytes.
 * @return value of type T extracted from buffer
 */
template<typename T> T get_data(const std::vector<uint8_t> &data, size_t buffer_offset) {
  if (sizeof(T) == sizeof(uint8_t)) {
    return T(data[buffer_offset]);
  }
  if (sizeof(T) == sizeof(uint16_t)) {
    return T((uint16_t(data[buffer_offset + 0]) << 8) | (uint16_t(data[buffer_offset + 1]) << 0));
  }

  if (sizeof(T) == sizeof(uint32_t)) {
    return static_cast<uint32_t>(get_data<uint16_t>(data, buffer_offset)) << 16 |
           static_cast<uint32_t>(get_data<uint16_t>(data, buffer_offset + 2));
  }

  if (sizeof(T) == sizeof(uint64_t)) {
    return static_cast<uint64_t>(get_data<uint32_t>(data, buffer_offset)) << 32 |
           (static_cast<uint64_t>(get_data<uint32_t>(data, buffer_offset + 4)));
  }

  static_assert(sizeof(T) == sizeof(uint8_t) || sizeof(T) == sizeof(uint16_t) || sizeof(T) == sizeof(uint32_t) ||
                    sizeof(T) == sizeof(uint64_t),
                "Unsupported type size in get_data; only 1, 2, 4, or 8-byte integer types are supported.");

  return T{};
}

/** Extract coil data from modbus response buffer
 * Responses for coil are packed into bytes .
 * coil 3 is bit 3 of the first response byte
 * coil 9 is bit 2 of the second response byte
 * @param coil number of the cil
 * @param data modbus response buffer (uint8_t)
 * @return content of coil register
 */
inline bool coil_from_vector(int coil, const std::vector<uint8_t> &data) {
  auto data_byte = coil / 8;
  return (data[data_byte] & (1 << (coil % 8))) > 0;
}

/** Extract bits from value and shift right according to the bitmask
 * if the bitmask is 0x00F0  we want the values frrom bit 5 - 8.
 * the result is then shifted right by the position if the first right set bit in the mask
 * Useful for modbus data where more than one value is packed in a 16 bit register
 * Example: on Epever the "Length of night" register 0x9065 encodes values of the whole night length of time as
 * D15 - D8 =  hour, D7 - D0 = minute
 * To get the hours use mask 0xFF00 and  0x00FF for the minute
 * @param data an integral value between 16 aand 32 bits,
 * @param bitmask the bitmask to apply
 */
template<typename N> N mask_and_shift_by_rightbit(N data, uint32_t mask) {
  auto result = (mask & data);
  if (result == 0 || mask == 0xFFFFFFFF) {
    return result;
  }
  for (size_t pos = 0; pos < sizeof(N) << 3; pos++) {
    if (pos < 32 && (mask & (1UL << pos)) != 0)
      return result >> pos;
  }
  return 0;
}

/** Convert float value to vector<uint16_t> suitable for sending
 * @param data target for payload
 * @param value float value to convert
 * @param value_type defines if 16/32 or FP32 is used
 * @return vector containing the modbus register words in correct order
 */
void number_to_payload(std::vector<uint16_t> &data, int64_t value, SensorValueType value_type);

/** Convert vector<uint8_t> response payload to number.
 * @param data payload with the data to convert
 * @param sensor_value_type defines if 16/32/64 bits or FP32 is used
 * @param offset offset to the data in data
 * @param bitmask bitmask used for masking and shifting
 * @return 64-bit number of the payload
 */
int64_t payload_to_number(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                          uint32_t bitmask);

inline std::vector<uint16_t> float_to_payload(float value, SensorValueType value_type) {
  int64_t val;

  if (value_type_is_float(value_type)) {
    val = bit_cast<uint32_t>(value);
  } else {
    val = llroundf(value);
  }

  std::vector<uint16_t> data;
  number_to_payload(data, val, value_type);
  return data;
}

}  // namespace esphome::modbus::helpers
