#pragma once

#include "esphome/core/component.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/automation.h"

#include <list>
#include <queue>
#include <set>
#include <utility>
#include <vector>

namespace esphome::modbus_controller {

class ModbusController;

using modbus::ModbusFunctionCode;
using modbus::ModbusRegisterType;
using modbus::ModbusExceptionCode;
using modbus::helpers::SensorValueType;

// Remove before 2026.10.0 — these helpers have moved to modbus::helpers
ESPDEPRECATED("Use modbus::helpers::value_type_is_float() instead. Removed in 2026.10.0", "2026.4.0")
inline bool value_type_is_float(SensorValueType v) { return modbus::helpers::value_type_is_float(v); }

ESPDEPRECATED("Use modbus::helpers::modbus_register_read_function() instead. Removed in 2026.10.0", "2026.4.0")
inline ModbusFunctionCode modbus_register_read_function(ModbusRegisterType reg_type) {
  return modbus::helpers::modbus_register_read_function(reg_type);
}

ESPDEPRECATED("Use modbus::helpers::modbus_register_write_function() instead. Removed in 2026.10.0", "2026.4.0")
inline ModbusFunctionCode modbus_register_write_function(ModbusRegisterType reg_type) {
  return modbus::helpers::modbus_register_write_function(reg_type);
}

ESPDEPRECATED("Use modbus::helpers::c_to_hex() instead. Removed in 2026.10.0", "2026.4.0")
inline uint8_t c_to_hex(char c) { return modbus::helpers::c_to_hex(c); }

ESPDEPRECATED("Use modbus::helpers::byte_from_hex_str() instead. Removed in 2026.10.0", "2026.4.0")
inline uint8_t byte_from_hex_str(const std::string &value, uint8_t pos) {
  return modbus::helpers::byte_from_hex_str(value, pos);
}

ESPDEPRECATED("Use modbus::helpers::word_from_hex_str() instead. Removed in 2026.10.0", "2026.4.0")
inline uint16_t word_from_hex_str(const std::string &value, uint8_t pos) {
  return modbus::helpers::word_from_hex_str(value, pos);
}

ESPDEPRECATED("Use modbus::helpers::dword_from_hex_str() instead. Removed in 2026.10.0", "2026.4.0")
inline uint32_t dword_from_hex_str(const std::string &value, uint8_t pos) {
  return modbus::helpers::dword_from_hex_str(value, pos);
}

ESPDEPRECATED("Use modbus::helpers::qword_from_hex_str() instead. Removed in 2026.10.0", "2026.4.0")
inline uint64_t qword_from_hex_str(const std::string &value, uint8_t pos) {
  return modbus::helpers::qword_from_hex_str(value, pos);
}

template<typename T>
ESPDEPRECATED("Use modbus::helpers::get_data() instead. Removed in 2026.10.0", "2026.4.0")
T get_data(const std::vector<uint8_t> &data, size_t buffer_offset) {
  return modbus::helpers::get_data<T>(data, buffer_offset);
}

ESPDEPRECATED("Use modbus::helpers::coil_from_vector() instead. Removed in 2026.10.0", "2026.4.0")
inline bool coil_from_vector(int coil, const std::vector<uint8_t> &data) {
  return modbus::helpers::coil_from_vector(coil, data);
}

template<typename N>
ESPDEPRECATED("Use modbus::helpers::mask_and_shift_by_rightbit() instead. Removed in 2026.10.0", "2026.4.0")
N mask_and_shift_by_rightbit(N data, uint32_t mask) {
  return modbus::helpers::mask_and_shift_by_rightbit(data, mask);
}

ESPDEPRECATED("Use modbus::helpers::number_to_payload() instead. Removed in 2026.10.0", "2026.4.0")
inline void number_to_payload(std::vector<uint16_t> &data, int64_t value, SensorValueType value_type) {
  modbus::helpers::number_to_payload(data, value, value_type);
}

ESPDEPRECATED("Use modbus::helpers::payload_to_number() instead. Removed in 2026.10.0", "2026.4.0")
inline int64_t payload_to_number(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                                 uint32_t bitmask) {
  return modbus::helpers::payload_to_number(data, sensor_value_type, offset, bitmask);
}

ESPDEPRECATED("Use modbus::helpers::float_to_payload() instead. Removed in 2026.10.0", "2026.4.0")
inline std::vector<uint16_t> float_to_payload(float value, SensorValueType value_type) {
  return modbus::helpers::float_to_payload(value, value_type);
}

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
  ModbusRegisterType register_type{ModbusRegisterType::CUSTOM};
  SensorValueType sensor_value_type{SensorValueType::RAW};
  uint16_t start_address{0};
  uint32_t bitmask{0};
  uint8_t offset{0};
  uint8_t register_count{0};
  uint8_t response_bytes{0};
  uint16_t skip_updates{0};
  std::vector<uint8_t> custom_data{};
  bool force_new_range{false};
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
  ModbusController *modbusdevice{nullptr};
  uint16_t register_address{0};
  uint16_t register_count{0};
  ModbusFunctionCode function_code{ModbusFunctionCode::CUSTOM};
  ModbusRegisterType register_type{ModbusRegisterType::CUSTOM};
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
  /// called when a modbus response was parsed without errors
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  /// called when a modbus error response was received
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
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
  template<typename F> void add_on_command_sent_callback(F &&callback) {
    this->command_sent_callback_.add(std::forward<F>(callback));
  }
  /// Set callback for online changes
  template<typename F> void add_on_online_callback(F &&callback) {
    this->online_callback_.add(std::forward<F>(callback));
  }
  /// Set callback for offline changes
  template<typename F> void add_on_offline_callback(F &&callback) {
    this->offline_callback_.add(std::forward<F>(callback));
  }
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
  /// Continuous range of modbus registers
  std::vector<RegisterRange> register_ranges_{};
  /// Hold the pending requests to be sent
  std::list<std::unique_ptr<ModbusCommandItem>> command_queue_;
  /// modbus response data waiting to get processed
  std::queue<std::unique_ptr<ModbusCommandItem>> incoming_queue_;
  /// if duplicate commands can be sent
  bool allow_duplicate_commands_{false};
  /// when was the last send operation
  uint32_t last_command_timestamp_{0};
  /// min time in ms between sending modbus commands
  uint16_t command_throttle_{0};
  /// if module didn't respond the last command
  bool module_offline_{false};
  /// how many updates to skip if module is offline
  uint16_t offline_skip_updates_{0};
  /// How many times we will retry a command if we get no response
  uint8_t max_cmd_retries_{4};
  /// Command sent callback
  CallbackManager<void(int, int)> command_sent_callback_{};
  /// Server online callback
  CallbackManager<void(int, int)> online_callback_{};
  /// Server offline callback
  CallbackManager<void(int, int)> offline_callback_{};
};

/** Convert vector<uint8_t> response payload to float.
 * @param data payload with data
 * @param item SensorItem object
 * @return float value of data
 */
inline float payload_to_float(const std::vector<uint8_t> &data, const SensorItem &item) {
  int64_t number = modbus::helpers::payload_to_number(data, item.sensor_value_type, item.offset, item.bitmask);

  float float_value;
  if (modbus::helpers::value_type_is_float(item.sensor_value_type)) {
    float_value = bit_cast<float>(static_cast<uint32_t>(number));
  } else {
    float_value = static_cast<float>(number);
  }

  return float_value;
}

}  // namespace esphome::modbus_controller
