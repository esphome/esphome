#pragma once

#include "esphome/core/component.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/automation.h"

#include <list>
#include <queue>
#include <set>
#include <utility>
#include <vector>

namespace esphome {
namespace modbus_server {

class ModbusServer;

using modbus::ModbusFunctionCode;
using modbus::ModbusRegisterType;
using modbus::ModbusExceptionCode;

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

class ModbusServer;

struct ServerCourtesyResponse {
  bool enabled{false};
  uint16_t register_last_address{0xFFFF};
  uint16_t register_value{0};
};

class ServerRegister {
  using ReadLambda = std::function<int64_t()>;
  using WriteLambda = std::function<bool(int64_t value)>;

 public:
  ServerRegister(uint16_t address, SensorValueType value_type, uint8_t register_count) {
    this->address = address;
    this->value_type = value_type;
    this->register_count = register_count;
  }

  template<typename T> void set_read_lambda(const std::function<T(uint16_t address)> &&user_read_lambda) {
    this->read_lambda = [this, user_read_lambda]() -> int64_t {
      T user_value = user_read_lambda(this->address);
      if constexpr (std::is_same_v<T, float>) {
        return bit_cast<uint32_t>(user_value);
      } else {
        return static_cast<int64_t>(user_value);
      }
    };
  }

  template<typename T>
  void set_write_lambda(const std::function<bool(uint16_t address, const T v)> &&user_write_lambda) {
    this->write_lambda = [this, user_write_lambda](int64_t number) {
      if constexpr (std::is_same_v<T, float>) {
        float float_value = bit_cast<float>(static_cast<uint32_t>(number));
        return user_write_lambda(this->address, float_value);
      }
      return user_write_lambda(this->address, static_cast<T>(number));
    };
  }

  // Formats a raw value into a string representation based on the value type for debugging
  std::string format_value(int64_t value) const {
    switch (this->value_type) {
      case SensorValueType::U_WORD:
      case SensorValueType::U_DWORD:
      case SensorValueType::U_DWORD_R:
      case SensorValueType::U_QWORD:
      case SensorValueType::U_QWORD_R:
        return std::to_string(static_cast<uint64_t>(value));
      case SensorValueType::S_WORD:
      case SensorValueType::S_DWORD:
      case SensorValueType::S_DWORD_R:
      case SensorValueType::S_QWORD:
      case SensorValueType::S_QWORD_R:
        return std::to_string(value);
      case SensorValueType::FP32_R:
      case SensorValueType::FP32:
        return str_sprintf("%.1f", bit_cast<float>(static_cast<uint32_t>(value)));
      default:
        return std::to_string(value);
    }
  }

  uint16_t address{0};
  SensorValueType value_type{SensorValueType::RAW};
  uint8_t register_count{0};
  ReadLambda read_lambda;
  WriteLambda write_lambda;
};

class ModbusServer : public Component, public modbus::ModbusServerDevice {
 public:
  void dump_config() override;

  /// Registers a server register with the controller. Called by esphomes code generator
  void add_server_register(ServerRegister *server_register) { server_registers_.push_back(server_register); }
  /// called when a modbus request (function code 0x03 or 0x04) was parsed without errors
  void on_modbus_read_registers(uint8_t function_code, uint16_t start_address, uint16_t number_of_registers) final;
  /// called when a modbus request (function code 0x06 or 0x10) was parsed without errors
  void on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data) final;
  /// Called by esphome generated code to set the server courtesy response object
  void set_server_courtesy_response(const ServerCourtesyResponse &server_courtesy_response) {
    this->server_courtesy_response_ = server_courtesy_response;
  }
  /// Get the server courtesy response object
  ServerCourtesyResponse get_server_courtesy_response() const { return this->server_courtesy_response_; }

 protected:
  /// Collection of all server registers for this component
  std::vector<ServerRegister *> server_registers_{};
  /// Server courtesy response
  ServerCourtesyResponse server_courtesy_response_{
      .enabled = false, .register_last_address = 0xFFFF, .register_value = 0};
};

}  // namespace modbus_server
}  // namespace esphome
