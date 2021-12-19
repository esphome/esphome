#pragma once

#include "esphome/core/component.h"

#include "esphome/core/automation.h"
#include "esphome/components/modbus/modbus.h"

#include <list>
#include <map>
#include <queue>
#include <vector>

namespace esphome {
namespace modbus_controller {

class ModbusController;

enum class ModbusFunctionCode {
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

enum class ModbusRegisterType : int {
  CUSTOM = 0x0,
  COIL = 0x01,
  DISCRETE_INPUT = 0x02,
  HOLDING = 0x03,
  READ = 0x04,
};

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

struct RegisterRange {
  uint16_t start_address;
  ModbusRegisterType register_type;
  uint8_t register_count;
  uint8_t skip_updates;  // the config value
  uint64_t first_sensorkey;
  uint8_t skip_updates_counter;  // the running value
} __attribute__((packed));

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

/** All sensors are stored in a map
 * to enable binary sensors for values encoded as bits in the same register the key of each sensor
 * the key is a 64 bit integer that combines the register properties
 * sensormap_ is sorted by this key. The key ensures the correct order when creating consequtive ranges
 * Format:  function_code (8 bit) | start address (16 bit)| offset (8bit)| bitmask (32 bit)
 */
inline uint64_t calc_key(ModbusRegisterType register_type, uint16_t start_address, uint8_t offset = 0,
                         uint32_t bitmask = 0) {
  return uint64_t((uint16_t(register_type) << 24) + (uint32_t(start_address) << 8) + (offset & 0xFF)) << 32 | bitmask;
}
inline uint16_t register_from_key(uint64_t key) { return (key >> 40) & 0xFFFF; }

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
 * the result is then shifted right by the postion if the first right set bit in the mask
 * Usefull for modbus data where more than one value is packed in a 16 bit register
 * Example: on Epever the "Length of night" register 0x9065 encodes values of the whole night length of time as
 * D15 - D8 =  hour, D7 - D0 = minute
 * To get the hours use mask 0xFF00 and  0x00FF for the minute
 * @param data an integral value between 16 aand 32 bits,
 * @param bitmask the bitmask to apply
 */
template<typename N> N mask_and_shift_by_rightbit(N data, uint32_t mask) {
  auto result = (mask & data);
  if (result == 0) {
    return result;
  }
  for (size_t pos = 0; pos < sizeof(N) << 3; pos++) {
    if ((mask & (1 << pos)) != 0)
      return result >> pos;
  }
  return 0;
}

/** convert float value to vector<uint16_t> suitable for sending
 * @param value float value to cconvert
 * @param value_type defines if 16/32 or FP32 is used
 * @return vector containing the modbus register words in correct order
 */
std::vector<uint16_t> float_to_payload(float value, SensorValueType value_type);

/** convert vector<uint8_t> response payload to float
 * @param value float value to cconvert
 * @param sensor_value_type defines if 16/32/64 bits or FP32 is used
 * @param offset offset to the data in data
 * @param bitmask bitmask used for masking and shifting
 * @return float version of the input
 */
float payload_to_float(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                       uint32_t bitmask);

class ModbusController;

class SensorItem {
 public:
  virtual void parse_and_publish(const std::vector<uint8_t> &data) = 0;

  void set_custom_data(const std::vector<uint8_t> &data) { custom_data = data; }
  uint64_t getkey() const { return calc_key(register_type, start_address, offset, bitmask); }
  size_t virtual get_register_size() const {
    if (register_type == ModbusRegisterType::COIL || register_type == ModbusRegisterType::DISCRETE_INPUT)
      return 1;
    else  // if CONF_RESPONSE_BYTES is used override the default
      return response_bytes > 0 ? response_bytes : register_count * 2;
  }
  // Override register size for modbus devices not using 1 register for one dword
  void set_register_size(uint8_t register_size) { response_bytes = register_size; }
  ModbusRegisterType register_type;
  SensorValueType sensor_value_type;
  uint16_t start_address;
  uint32_t bitmask;
  uint8_t offset;
  uint8_t register_count;
  uint8_t response_bytes{0};
  uint8_t skip_updates;
  std::vector<uint8_t> custom_data{};
  bool force_new_range{false};
};

class ModbusCommandItem {
 public:
  static const size_t MAX_PAYLOAD_BYTES = 240;
  static const uint8_t MAX_SEND_REPEATS = 5;
  ModbusController *modbusdevice;
  uint16_t register_address;
  uint16_t register_count;
  ModbusFunctionCode function_code;
  ModbusRegisterType register_type;
  std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
      on_data_func;
  std::vector<uint8_t> payload = {};
  bool send();
  // wrong commands (esp. custom commands) can block the send queue
  // limit the number of repeats
  uint8_t send_countdown{MAX_SEND_REPEATS};
  /// factory methods
  /** Create modbus read command
   *  Function code 02-04
   * @param modbusdevice pointer to the device to execute the command
   * @param function_code modbus function code for the read command
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @param handler function called when the response is received
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_read_command(
      ModbusController *modbusdevice, ModbusRegisterType register_type, uint16_t start_address, uint16_t register_count,
      std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler);
  /** Create modbus read command
   *  Function code 02-04
   * @param modbusdevice pointer to the device to execute the command
   * @param function_code modbus function code for the read command
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_read_command(ModbusController *modbusdevice, ModbusRegisterType register_type,
                                               uint16_t start_address, uint16_t register_count);
  /** Create modbus read command
   *  Function code 02-04
   * @param modbusdevice pointer to the device to execute the command
   * @param function_code modbus function code for the read command
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @param handler function called when the response is received
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_multiple_command(ModbusController *modbusdevice, uint16_t start_address,
                                                         uint16_t register_count, const std::vector<uint16_t> &values);
  /** Create modbus write multiple registers command
   *  Function 16 (10hex) Write Multiple Registers
   * @param modbusdevice pointer to the device to execute the command
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @param values uint16_t array to be written to the registers
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_single_command(ModbusController *modbusdevice, uint16_t start_address,
                                                       int16_t value);
  /** Create modbus write single registers command
   *  Function 05 (05hex) Write Single Coil
   * @param modbusdevice pointer to the device to execute the command
   * @param start_address modbus address of the first register to read
   * @param value uint16_t data to be written to the registers
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_single_coil(ModbusController *modbusdevice, uint16_t address, bool value);

  /** Create modbus write multiple registers command
   *  Function 15 (0Fhex) Write Multiple Coils
   * @param modbusdevice pointer to the device to execute the command
   * @param start_address modbus address of the first register to read
   * @param value bool vector of values to be written to the registers
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_multiple_coils(ModbusController *modbusdevice, uint16_t start_address,
                                                       const std::vector<bool> &values);
  /** Create custom modbus command
   * @param modbusdevice pointer to the device to execute the command
   * @param values byte vector of data to be sent to the device. The compplete payload must be provided with the
   * exception of the crc codess
   * @param handler function called when the response is received. Default is just logging a response
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_custom_command(
      ModbusController *modbusdevice, const std::vector<uint8_t> &values,
      std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler = nullptr);
};

/** Modbus controller class.
 *   Each instance handles the modbus commuinication for all sensors with the same modbus address
 *
 * all sensor items (sensors, switches, binarysensor ...) are parsed in modbus address ranges.
 * when esphome calls ModbusController::Update the commands for each range are created and sent
 * Responses for the commands are dispatched to the modbus sensor items.
 */

class ModbusController : public PollingComponent, public modbus::ModbusDevice {
 public:
  ModbusController(uint16_t throttle = 0) : modbus::ModbusDevice(), command_throttle_(throttle){};
  void dump_config() override;
  void loop() override;
  void setup() override;
  void update() override;

  /// queues a modbus command in the send queue
  void queue_command(const ModbusCommandItem &command);
  /// Registers a sensor with the controller. Called by esphomes code generator
  void add_sensor_item(SensorItem *item) { sensormap_[item->getkey()] = item; }
  /// called when a modbus response was prased without errors
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  /// called when a modbus error response was received
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
  /// default delegate called by process_modbus_data when a response has retrieved from the incoming queue
  void on_register_data(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data);
  /// default delegate called by process_modbus_data when a response for a write response has retrieved from the
  /// incoming queue
  void on_write_register_response(ModbusRegisterType register_type, uint16_t start_address,
                                  const std::vector<uint8_t> &data);
  /// called by esphome generated code to set the command_throttle period
  void set_command_throttle(uint16_t command_throttle) { this->command_throttle_ = command_throttle; }

 protected:
  /// parse sensormap_ and create range of sequential addresses
  size_t create_register_ranges_();
  // find register in sensormap. Returns iterator with all registers having the same start address
  std::map<uint64_t, SensorItem *>::iterator find_register_(ModbusRegisterType register_type, uint16_t start_address);
  /// submit the read command for the address range to the send queue
  void update_range_(RegisterRange &r);
  /// parse incoming modbus data
  void process_modbus_data_(const ModbusCommandItem *response);
  /// send the next modbus command from the send queue
  bool send_next_command_();
  /// get the number of queued modbus commands (should be mostly empty)
  size_t get_command_queue_length_() { return command_queue_.size(); }
  /// dump the parsed sensormap for diagnostics
  void dump_sensormap_();
  /// Collection of all sensors for this component
  /// see calc_key how the key is contructed
  std::map<uint64_t, SensorItem *> sensormap_;
  /// Continous range of modbus registers
  std::vector<RegisterRange> register_ranges_;
  /// Hold the pending requests to be sent
  std::list<std::unique_ptr<ModbusCommandItem>> command_queue_;
  /// modbus response data waiting to get processed
  std::queue<std::unique_ptr<ModbusCommandItem>> incoming_queue_;
  /// when was the last send operation
  uint32_t last_command_timestamp_;
  /// min time in ms between sending modbus commands
  uint16_t command_throttle_;
};

/** convert vector<uint8_t> response payload to float
 * @param value float value to cconvert
 * @param item SensorItem object
 * @return float version of the input
 */
inline float payload_to_float(const std::vector<uint8_t> &data, const SensorItem &item) {
  return payload_to_float(data, item.sensor_value_type, item.offset, item.bitmask);
}

}  // namespace modbus_controller
}  // namespace esphome
