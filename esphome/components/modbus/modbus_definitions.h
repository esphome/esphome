#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace modbus {

/// Modbus definitions from specs:
/// https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf
// 5 Function Code Categories
const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_1_INIT = 65;  // 0x41
const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_1_END = 72;   // 0x48

const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_2_INIT = 100;  // 0x64
const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_2_END = 110;   // 0x6E

enum class ModbusFunctionCode : uint8_t {
  CUSTOM = 0x00,
  READ_COILS = 0x01,
  READ_DISCRETE_INPUTS = 0x02,
  READ_HOLDING_REGISTERS = 0x03,
  READ_INPUT_REGISTERS = 0x04,
  WRITE_SINGLE_COIL = 0x05,
  WRITE_SINGLE_REGISTER = 0x06,
  READ_EXCEPTION_STATUS = 0x07,   // not implemented
  DIAGNOSTICS = 0x08,             // not implemented
  GET_COMM_EVENT_COUNTER = 0x0B,  // not implemented
  GET_COMM_EVENT_LOG = 0x0C,      // not implemented
  WRITE_MULTIPLE_COILS = 0x0F,
  WRITE_MULTIPLE_REGISTERS = 0x10,
  REPORT_SERVER_ID = 0x11,               // not implemented
  READ_FILE_RECORD = 0x14,               // not implemented
  WRITE_FILE_RECORD = 0x15,              // not implemented
  MASK_WRITE_REGISTER = 0x16,            // not implemented
  READ_WRITE_MULTIPLE_REGISTERS = 0x17,  // not implemented
  READ_FIFO_QUEUE = 0x18,                // not implemented
};

/*Allow  comparison operators between ModbusFunctionCode and uint8_t*/
inline bool operator==(ModbusFunctionCode lhs, uint8_t rhs) { return static_cast<uint8_t>(lhs) == rhs; }
inline bool operator==(uint8_t lhs, ModbusFunctionCode rhs) { return lhs == static_cast<uint8_t>(rhs); }
inline bool operator!=(ModbusFunctionCode lhs, uint8_t rhs) { return !(static_cast<uint8_t>(lhs) == rhs); }
inline bool operator!=(uint8_t lhs, ModbusFunctionCode rhs) { return !(lhs == static_cast<uint8_t>(rhs)); }
inline bool operator<(ModbusFunctionCode lhs, uint8_t rhs) { return static_cast<uint8_t>(lhs) < rhs; }
inline bool operator<(uint8_t lhs, ModbusFunctionCode rhs) { return lhs < static_cast<uint8_t>(rhs); }
inline bool operator<=(ModbusFunctionCode lhs, uint8_t rhs) { return static_cast<uint8_t>(lhs) <= rhs; }
inline bool operator<=(uint8_t lhs, ModbusFunctionCode rhs) { return lhs <= static_cast<uint8_t>(rhs); }
inline bool operator>(ModbusFunctionCode lhs, uint8_t rhs) { return static_cast<uint8_t>(lhs) > rhs; }
inline bool operator>(uint8_t lhs, ModbusFunctionCode rhs) { return lhs > static_cast<uint8_t>(rhs); }
inline bool operator>=(ModbusFunctionCode lhs, uint8_t rhs) { return static_cast<uint8_t>(lhs) >= rhs; }
inline bool operator>=(uint8_t lhs, ModbusFunctionCode rhs) { return lhs >= static_cast<uint8_t>(rhs); }

// 4.3 MODBUS Data model
enum class ModbusRegisterType : uint8_t {
  CUSTOM = 0x00,
  COIL = 0x01,
  DISCRETE_INPUT = 0x02,
  HOLDING = 0x03,
  READ = 0x04,
};

// 7 MODBUS Exception Responses:
const uint8_t FUNCTION_CODE_MASK = 0x7F;
const uint8_t FUNCTION_CODE_EXCEPTION_MASK = 0x80;

enum class ModbusExceptionCode : uint8_t {
  ILLEGAL_FUNCTION = 0x01,
  ILLEGAL_DATA_ADDRESS = 0x02,
  ILLEGAL_DATA_VALUE = 0x03,
  SERVICE_DEVICE_FAILURE = 0x04,
  ACKNOWLEDGE = 0x05,
  SERVER_DEVICE_BUSY = 0x06,
  MEMORY_PARITY_ERROR = 0x08,
  GATEWAY_PATH_UNAVAILABLE = 0x0A,
  GATEWAY_TARGET_DEVICE_FAILED_TO_RESPOND = 0x0B,
};

// 6.12 16 (0x10) Write Multiple registers:
const uint8_t MAX_NUM_OF_REGISTERS_TO_WRITE = 123;  // 0x7B

// 6.3 03 (0x03) Read Holding Registers
// 6.4 04 (0x04) Read Input Registers
const uint8_t MAX_NUM_OF_REGISTERS_TO_READ = 125;  // 0x7D

const uint16_t MAX_FRAME_SIZE = 256;

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
      break;
    case ModbusRegisterType::DISCRETE_INPUT:
      return ModbusFunctionCode::READ_DISCRETE_INPUTS;
      break;
    case ModbusRegisterType::HOLDING:
      return ModbusFunctionCode::READ_HOLDING_REGISTERS;
      break;
    case ModbusRegisterType::READ:
      return ModbusFunctionCode::READ_INPUT_REGISTERS;
      break;
    default:
      return ModbusFunctionCode::CUSTOM;
      break;
  }
}
inline ModbusFunctionCode modbus_register_write_function(ModbusRegisterType reg_type) {
  switch (reg_type) {
    case ModbusRegisterType::COIL:
      return ModbusFunctionCode::WRITE_SINGLE_COIL;
      break;
    case ModbusRegisterType::DISCRETE_INPUT:
      return ModbusFunctionCode::CUSTOM;
      break;
    case ModbusRegisterType::HOLDING:
      return ModbusFunctionCode::READ_WRITE_MULTIPLE_REGISTERS;
      break;
    case ModbusRegisterType::READ:
    default:
      return ModbusFunctionCode::CUSTOM;
      break;
  }
}

inline uint8_t c_to_hex(char c) { return (c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0'); }

/** Get a byte from a hex string
 *  hex_byte_from_str("1122",1) returns uint_8 value 0x22 == 34
 *  hex_byte_from_str("1122",0) returns 0x11
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return byte value
 */
inline uint8_t byte_from_hex_str(const std::string &value, uint8_t pos) {
  if (value.length() < pos * 2 + 1)
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
    return get_data<uint16_t>(data, buffer_offset) << 16 | get_data<uint16_t>(data, (buffer_offset + 2));
  }

  if (sizeof(T) == sizeof(uint64_t)) {
    return static_cast<uint64_t>(get_data<uint32_t>(data, buffer_offset)) << 32 |
           (static_cast<uint64_t>(get_data<uint32_t>(data, buffer_offset + 4)));
  }
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
    if ((mask & (1 << pos)) != 0)
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

/// End of Modbus definitions
}  // namespace modbus
}  // namespace esphome
