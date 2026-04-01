#pragma once

#include <string>
#include <vector>
#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/components/modbus/modbus_definitions.h"

namespace esphome::modbus::helpers {

inline bool is_function_code_read(uint8_t function_code) {
  uint8_t masked_function_code = static_cast<uint8_t>(function_code) & FUNCTION_CODE_MASK;
  return masked_function_code == ModbusFunctionCode::READ_COILS ||
         masked_function_code == ModbusFunctionCode::READ_DISCRETE_INPUTS ||
         masked_function_code == ModbusFunctionCode::READ_HOLDING_REGISTERS ||
         masked_function_code == ModbusFunctionCode::READ_INPUT_REGISTERS;
}

inline bool is_function_code_write(uint8_t function_code) {
  uint8_t masked_function_code = static_cast<uint8_t>(function_code) & FUNCTION_CODE_MASK;
  return masked_function_code == ModbusFunctionCode::WRITE_SINGLE_COIL ||
         masked_function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
         masked_function_code == ModbusFunctionCode::WRITE_MULTIPLE_COILS ||
         masked_function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS;
}

inline bool is_function_code_exception(uint8_t function_code) {
  return (static_cast<uint8_t>(function_code) & FUNCTION_CODE_EXCEPTION_MASK) != 0;
}

inline bool is_function_code_custom(uint8_t function_code) {
  uint8_t masked_function_code = static_cast<uint8_t>(function_code) & FUNCTION_CODE_MASK;
  return (masked_function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_1_INIT &&
          masked_function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_1_END) ||
         (masked_function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_2_INIT &&
          masked_function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_2_END);
}

inline bool is_register_type_binary(ModbusRegisterType type) {
  return type == ModbusRegisterType::COIL || type == ModbusRegisterType::DISCRETE_INPUT;
}

// Returns the expected length of a server response frame based on the function code
// If the frame is too short to determine the length, returns the minimum length
inline uint16_t server_frame_length(const std::vector<uint8_t> &frame) {
  if (frame.size() < 2)
    return MIN_FRAME_SIZE;
  if (is_function_code_exception(frame[1])) {
    return 5;  // address(1) + function(1) + exception(1) + CRC(2)
  }
  switch (static_cast<ModbusFunctionCode>(frame[1])) {
    case ModbusFunctionCode::READ_COILS:
    case ModbusFunctionCode::READ_DISCRETE_INPUTS:
    case ModbusFunctionCode::READ_HOLDING_REGISTERS:
    case ModbusFunctionCode::READ_INPUT_REGISTERS:
      // address(1) + function(1) + byte count(1) + data + CRC(2)
      return 5 + (frame.size() > 2 ? std::min(frame[2], uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2)) : 0);
    case ModbusFunctionCode::WRITE_SINGLE_COIL:
    case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
    case ModbusFunctionCode::WRITE_MULTIPLE_COILS:
    case ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS:
      return 8;  // address(1) + function(1) + output/register address(2) + value(2) + CRC(2)
    // Unsupported function codes. Included here to prevent parser failures. Excluding Serial Line specific functions.
    case ModbusFunctionCode::READ_FILE_RECORD:
    case ModbusFunctionCode::WRITE_FILE_RECORD:
      // address(1) + function(1) + byte count(1) + data + CRC(2)
      return 5 + (frame.size() > 2 ? std::min(frame[2], uint8_t(MAX_FRAME_SIZE - 5)) : 0);
    case ModbusFunctionCode::MASK_WRITE_REGISTER:
      return 10;  // address(1) + function(1) + reference address(2) + AND mask(2) + OR mask(2) + CRC(2)
    case ModbusFunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      // address(1) + function(1) + byte count(1) + data + CRC(2)
      return 5 + (frame.size() > 2 ? std::min(frame[2], uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2)) : 0);
    case ModbusFunctionCode::READ_FIFO_QUEUE:
      // address(1) + function(1) + fifo address(2) CRC(2)
      return 6;
    default:
      return MIN_FRAME_SIZE;  // unknown length
  }
}

// Returns the expected length of a client response frame based on the function code
// If the frame is too short to determine the length, returns the minimum length
inline uint16_t client_frame_length(const std::vector<uint8_t> &frame) {
  if (frame.size() < 2)
    return MIN_FRAME_SIZE;
  switch (static_cast<ModbusFunctionCode>(frame[1])) {
    case ModbusFunctionCode::READ_COILS:
    case ModbusFunctionCode::READ_DISCRETE_INPUTS:
    case ModbusFunctionCode::READ_HOLDING_REGISTERS:
    case ModbusFunctionCode::READ_INPUT_REGISTERS:
      // address(1) + function(1) + start address(2) + quantity(2) + CRC(2)
    case ModbusFunctionCode::WRITE_SINGLE_COIL:
    case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
      return 8;  // address(1) + function(1) + output/register address(2) + value(2) + CRC(2)
    case ModbusFunctionCode::WRITE_MULTIPLE_COILS:
    case ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS:
      // address(1) + function(1) + start address(2) + quantity(2) + byte count(1) + data + CRC(2)
      return 9 + (frame.size() > 6 ? std::min(frame[6], uint8_t(MAX_NUM_OF_REGISTERS_TO_WRITE * 2)) : 0);
    // Unsupported function codes. Included here to prevent parser failures. Excluding Serial Line specific functions.
    case ModbusFunctionCode::READ_FILE_RECORD:
    case ModbusFunctionCode::WRITE_FILE_RECORD:
      // address(1) + function(1) + byte count(1) + data + CRC(2)
      return 5 + (frame.size() > 2 ? std::min(frame[2], uint8_t(MAX_FRAME_SIZE - 5)) : 0);
    case ModbusFunctionCode::MASK_WRITE_REGISTER:
      return 10;  // address(1) + function(1) + reference address(2) + AND mask(2) + OR mask(2) + CRC(2)
    case ModbusFunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      // address(1) + function(1) + read start address(2) + read quantity(2) + write start address(2) +
      // write quantity(2) + byte count(1) + data + CRC(2)
      return 13 + (frame.size() > 10 ? std::min(frame[10], uint8_t(MAX_NUM_OF_REGISTERS_TO_WRITE * 2)) : 0);
    case ModbusFunctionCode::READ_FIFO_QUEUE:
      // address(1) + function(1) + fifo address(2) CRC(2)
      return 6;
    default:
      return MIN_FRAME_SIZE;  // unknown length
  }
}

inline uint8_t server_frame_data_offset(const std::vector<uint8_t> &frame) {
  if (frame.size() < 2)
    return 0;
  switch (static_cast<ModbusFunctionCode>(frame[1])) {
    case ModbusFunctionCode::READ_COILS:
    case ModbusFunctionCode::READ_DISCRETE_INPUTS:
    case ModbusFunctionCode::READ_HOLDING_REGISTERS:
    case ModbusFunctionCode::READ_INPUT_REGISTERS:
      return 3;  // address(1) + function(1) + byte count(1) + data + CRC(2)
    default:
      return 2;
  }
}

inline uint8_t client_frame_data_offset(const std::vector<uint8_t> &) { return 2; }

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

// Check frame length for supported read and write function codes.
inline bool is_client_frame_length_valid(const std::vector<uint8_t> &frame, bool has_crc = true) {
  uint16_t frame_length = frame.size() + (has_crc ? 0 : 2);  // Account for CRC if not already included in frame
  if (frame_length < MIN_FRAME_SIZE || frame_length > MAX_FRAME_SIZE)
    return false;
  if (is_function_code_read(frame[1]) || is_function_code_write(frame[1])) {
    return client_frame_length(frame) == frame_length;
  }
  return true;
}

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
      return ModbusFunctionCode::INVALID;
  }
}

inline ModbusFunctionCode modbus_register_write_function(ModbusRegisterType reg_type, bool multiple = false) {
  switch (reg_type) {
    case ModbusRegisterType::COIL:
      return multiple ? ModbusFunctionCode::WRITE_MULTIPLE_COILS : ModbusFunctionCode::WRITE_SINGLE_COIL;
    case ModbusRegisterType::HOLDING:
      return multiple ? ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS : ModbusFunctionCode::WRITE_SINGLE_REGISTER;
    // These register types can't be written (per spec)
    case ModbusRegisterType::READ:
    case ModbusRegisterType::DISCRETE_INPUT:
    default:
      return ModbusFunctionCode::INVALID;
  }
}

inline ModbusRegisterType modbus_register_type(ModbusFunctionCode function_code) {
  switch (function_code) {
    case ModbusFunctionCode::READ_COILS:
    case ModbusFunctionCode::WRITE_SINGLE_COIL:
    case ModbusFunctionCode::WRITE_MULTIPLE_COILS:
      return ModbusRegisterType::COIL;
    case ModbusFunctionCode::READ_DISCRETE_INPUTS:
      return ModbusRegisterType::DISCRETE_INPUT;
    case ModbusFunctionCode::READ_HOLDING_REGISTERS:
    case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
    case ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS:
    case ModbusFunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      return ModbusRegisterType::HOLDING;
    case ModbusFunctionCode::READ_INPUT_REGISTERS:
      return ModbusRegisterType::READ;
    default:
      return ModbusRegisterType::CUSTOM;
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
                          uint32_t bitmask, bool *error_return = nullptr);

/** Create a modbus clinet pdu for reading/writing single/multiple coils/register/inputs.
 * @param pdu target for modbus protocol data unit (function code + data)
 * @param function_code the modbus function code to use. One of:
 * READ_COILS
 * READ_DISCRETE_INPUTS
 * READ_HOLDING_REGISTERS
 * READ_INPUT_REGISTERS
 * WRITE_SINGLE_COIL
 * WRITE_SINGLE_REGISTER
 * WRITE_MULTIPLE_COILS
 * WRITE_MULTIPLE_REGISTERS
 * @param start_address coil/register/input starting ddress
 * @param number_of_entities number of coils/registers/inputs to read/write
 * @param values vector of the values to write.
 */
void create_client_pdu(std::vector<uint8_t> &pdu, ModbusFunctionCode function_code, uint16_t start_address,
                       uint16_t number_of_entities, const std::vector<uint8_t> &values = {});

/** Create modbus write multiple registers command
 *  Function 0x10 Write Multiple Registers
 * @param pdu target for modbus protocol data unit (function code + data)
 * @param start_address modbus address of the first register to read
 * @param register_count number of registers to read
 * @param values uint16_t register values to write
 */
void create_write_multiple_pdu(std::vector<uint8_t> &pdu, uint16_t start_address, uint16_t register_count,
                               const std::vector<uint16_t> &values);

/** Create modbus write single registers command
 *  Function 0x06 Write Single Register
 * @param pdu target for modbus protocol data unit (function code + data)
 * @param start_address modbus address of the first coil to read
 * @param value uint16_t data to be written to the coils
 */
void create_write_single_pdu(std::vector<uint8_t> &pdu, uint16_t start_address, uint16_t value);

/** Create modbus write single coil command
 *  Function 0x05 Write Single Coil
 * @param pdu target for modbus protocol data unit (function code + data)
 * @param start_address modbus address of the first coil to write
 * @param value data to be written to the coils
 */
void create_write_single_coil_pdu(std::vector<uint8_t> &pdu, uint16_t address, bool value);

/** Create modbus write multiple coils command
 *  Function 0x0F Write Multiple Coils
 * @param pdu target for modbus protocol data unit (function code + data)
 * @param start_address modbus address of the first register to read
 * @param value bool vector of values to be written to the coils
 */
void create_write_multiple_coils_pdu(std::vector<uint8_t> &pdu, uint16_t start_address,
                                     const std::vector<bool> &values);

std::vector<uint8_t> add_crc_to_payload(const std::vector<uint8_t> &payload);

/** Convert float to vector<uint8_t> response payload.
 * @param value value to convert
 * @param value_type  defines if 16/32/64 bits or FP32 is used
 * @return data payload with data
 */
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
