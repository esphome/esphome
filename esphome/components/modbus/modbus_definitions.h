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
/// End of Modbus definitions
}  // namespace modbus
}  // namespace esphome
