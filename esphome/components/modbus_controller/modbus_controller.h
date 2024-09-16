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

enum class ModbusRegisterType : uint8_t {
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

class ModbusController;

class SensorItem {
 public:
  virtual void parse_and_publish(const std::vector<uint8_t> &data) = 0;

  void set_custom_data(const std::vector<uint8_t> &data) { custom_data = data; }
  size_t virtual get_register_size() const {
    if (register_type == ModbusRegisterType::COIL || register_type == ModbusRegisterType::DISCRETE_INPUT) {
      return 1;
    } else {  // if CONF_RESPONSE_BYTES is used override the default
      return response_bytes > 0 ? response_bytes : register_count * 2;
    }
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
  uint16_t skip_updates;
  std::vector<uint8_t> custom_data{};
  bool force_new_range{false};
};

class ServerRegister {
 public:
  ServerRegister(uint16_t address, SensorValueType value_type, uint8_t register_count,
                 std::function<float()> read_lambda) {
    this->address = address;
    this->value_type = value_type;
    this->register_count = register_count;
    this->read_lambda = std::move(read_lambda);
  }
  uint16_t address;
  SensorValueType value_type;
  uint8_t register_count;
  std::function<float()> read_lambda;
};

// ModbusController::create_register_ranges_ tries to optimize register range
// for this the sensors must be ordered by register_type, start_address and bitmask
class SensorItemsComparator {
 public:
  bool operator()(const SensorItem *lhs, const SensorItem *rhs) const {
    // first sort according to register type
    if (lhs->register_type != rhs->register_type) {
      return lhs->register_type < rhs->register_type;
    }

    // ensure that sensor with force_new_range set are before the others
    if (lhs->force_new_range != rhs->force_new_range) {
      return lhs->force_new_range > rhs->force_new_range;
    }

    // sort by start address
    if (lhs->start_address != rhs->start_address) {
      return lhs->start_address < rhs->start_address;
    }

    // sort by offset (ensures update of sensors in ascending order)
    if (lhs->offset != rhs->offset) {
      return lhs->offset < rhs->offset;
    }

    // The pointer to the sensor is used last to ensure that
    // multiple sensors with the same values can be added with a stable sort order.
    return lhs < rhs;
  }
};

using SensorSet = std::set<SensorItem *, SensorItemsComparator>;

struct RegisterRange {
  uint16_t start_address;
  ModbusRegisterType register_type;
  uint8_t register_count;
  uint16_t skip_updates;          // the config value
  SensorSet sensors;              // all sensors of this range
  uint16_t skip_updates_counter;  // the running value
};

class ModbusCommandItem {
 public:
  static const size_t MAX_PAYLOAD_BYTES = 240;
  ModbusController *modbusdevice;
  uint16_t register_address;
  uint16_t register_count;
  ModbusFunctionCode function_code;
  ModbusRegisterType register_type;
  std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
      on_data_func;
  std::vector<uint8_t> payload = {};
  bool send();
  /// Check if the command should be retried based on the max_retries parameter
  bool should_retry(uint8_t max_retries) { return this->send_count_ <= max_retries; };

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
   * @param value uint16_t single register value to write
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_single_command(ModbusController *modbusdevice, uint16_t start_address,
                                                       uint16_t value);
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
   * @param values byte vector of data to be sent to the device. The complete payload must be provided with the
   * exception of the crc codes
   * @param handler function called when the response is received. Default is just logging a response
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_custom_command(
      ModbusController *modbusdevice, const std::vector<uint8_t> &values,
      std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler = nullptr);

  /** Create custom modbus command
   * @param modbusdevice pointer to the device to execute the command
   * @param values word vector of data to be sent to the device. The complete payload must be provided with the
   * exception of the crc codes
   * @param handler function called when the response is received. Default is just logging a response
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_custom_command(
      ModbusController *modbusdevice, const std::vector<uint16_t> &values,
      std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler = nullptr);

  bool is_equal(const ModbusCommandItem &other);

 protected:
  // wrong commands (esp. custom commands) can block the send queue, limit the number of repeats.
  /// How many times this command has been sent
  uint8_t send_count_{0};
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
  void dump_config() override;
  void loop() override;
  void setup() override;
  void update() override;

  /// queues a modbus command in the send queue
  void queue_command(const ModbusCommandItem &command);
  /// Registers a sensor with the controller. Called by esphomes code generator
  void add_sensor_item(SensorItem *item) { sensorset_.insert(item); }
  /// Registers a server register with the controller. Called by esphomes code generator
  void add_server_register(ServerRegister *server_register) { server_registers_.push_back(server_register); }
  /// called when a modbus response was parsed without errors
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  /// called when a modbus error response was received
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
  /// called when a modbus request (function code 3 or 4) was parsed without errors
  void on_modbus_read_registers(uint8_t function_code, uint16_t start_address, uint16_t number_of_registers) final;
  /// default delegate called by process_modbus_data when a response has retrieved from the incoming queue
  void on_register_data(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data);
  /// default delegate called by process_modbus_data when a response for a write response has retrieved from the
  /// incoming queue
  void on_write_register_response(ModbusRegisterType register_type, uint16_t start_address,
                                  const std::vector<uint8_t> &data);
  /// Allow a duplicate command to be sent
  void set_allow_duplicate_commands(bool allow_duplicate_commands) {
    this->allow_duplicate_commands_ = allow_duplicate_commands;
  }
  /// get if a duplicate command can be sent
  bool get_allow_duplicate_commands() { return this->allow_duplicate_commands_; }
  /// called by esphome generated code to set the command_throttle period
  void set_command_throttle(uint16_t command_throttle) { this->command_throttle_ = command_throttle; }
  /// called by esphome generated code to set the offline_skip_updates
  void set_offline_skip_updates(uint16_t offline_skip_updates) { this->offline_skip_updates_ = offline_skip_updates; }
  /// get the number of queued modbus commands (should be mostly empty)
  size_t get_command_queue_length() { return command_queue_.size(); }
  /// get if the module is offline, didn't respond the last command
  bool get_module_offline() { return module_offline_; }
  /// Set callback for commands
  void add_on_command_sent_callback(std::function<void(int, int)> &&callback);
  /// called by esphome generated code to set the max_cmd_retries.
  void set_max_cmd_retries(uint8_t max_cmd_retries) { this->max_cmd_retries_ = max_cmd_retries; }
  /// get how many times a command will be (re)sent if no response is received
  uint8_t get_max_cmd_retries() { return this->max_cmd_retries_; }

 protected:
  /// parse sensormap_ and create range of sequential addresses
  size_t create_register_ranges_();
  // find register in sensormap. Returns iterator with all registers having the same start address
  SensorSet find_sensors_(ModbusRegisterType register_type, uint16_t start_address) const;
  /// submit the read command for the address range to the send queue
  void update_range_(RegisterRange &r);
  /// parse incoming modbus data
  void process_modbus_data_(const ModbusCommandItem *response);
  /// send the next modbus command from the send queue
  bool send_next_command_();
  /// dump the parsed sensormap for diagnostics
  void dump_sensors_();
  /// Collection of all sensors for this component
  SensorSet sensorset_;
  /// Collection of all server registers for this component
  std::vector<ServerRegister *> server_registers_;
  /// Continuous range of modbus registers
  std::vector<RegisterRange> register_ranges_;
  /// Hold the pending requests to be sent
  std::list<std::unique_ptr<ModbusCommandItem>> command_queue_;
  /// modbus response data waiting to get processed
  std::queue<std::unique_ptr<ModbusCommandItem>> incoming_queue_;
  /// if duplicate commands can be sent
  bool allow_duplicate_commands_;
  /// when was the last send operation
  uint32_t last_command_timestamp_;
  /// min time in ms between sending modbus commands
  uint16_t command_throttle_;
  /// if module didn't respond the last command
  bool module_offline_;
  /// how many updates to skip if module is offline
  uint16_t offline_skip_updates_;
  /// How many times we will retry a command if we get no response
  uint8_t max_cmd_retries_{4};
  CallbackManager<void(int, int)> command_sent_callback_{};
};

/** Convert vector<uint8_t> response payload to float.
 * @param data payload with data
 * @param item SensorItem object
 * @return float value of data
 */
inline float payload_to_float(const std::vector<uint8_t> &data, const SensorItem &item) {
  int64_t number = payload_to_number(data, item.sensor_value_type, item.offset, item.bitmask);

  float float_value;
  if (item.sensor_value_type == SensorValueType::FP32 || item.sensor_value_type == SensorValueType::FP32_R) {
    float_value = bit_cast<float>(static_cast<uint32_t>(number));
  } else {
    float_value = static_cast<float>(number);
  }

  return float_value;
}

inline std::vector<uint16_t> float_to_payload(float value, SensorValueType value_type) {
  int64_t val;

  if (value_type == SensorValueType::FP32 || value_type == SensorValueType::FP32_R) {
    val = bit_cast<uint32_t>(value);
  } else {
    val = llroundf(value);
  }

  std::vector<uint16_t> data;
  number_to_payload(data, val, value_type);
  return data;
}

}  // namespace modbus_controller
}  // namespace esphome
