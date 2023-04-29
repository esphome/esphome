#pragma once

#include "esphome/core/component.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/automation.h"

#include <list>
#include <queue>
#include <set>
#include <vector>

namespace esphome {
namespace modbus_controller {

class ModbusController;

class SensorItem : public modbus::SensorItem{
 public:
  virtual void parse_and_publish(const std::vector<uint8_t> &data) = 0;
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
using ModbusRegisterType = modbus::ModbusRegisterType;
using SensorValueType = modbus::SensorValueType;

struct RegisterRange {
  uint16_t start_address;
  modbus::ModbusRegisterType register_type;
  uint8_t register_count;
  uint16_t skip_updates;          // the config value
  SensorSet sensors;              // all sensors of this range
  uint16_t skip_updates_counter;  // the running value
};

class ModbusCommandItem {
 public:
  static const size_t MAX_PAYLOAD_BYTES = 240;
  static const uint8_t MAX_SEND_REPEATS = 5;
  ModbusController *modbusdevice;
  uint16_t register_address;
  uint16_t register_count;
  modbus::ModbusFunctionCode function_code;
  modbus::ModbusRegisterType register_type;
  std::function<void(modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
      on_data_func;
  std::vector<uint8_t> payload = {};
  bool is_response{false};
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
      ModbusController *modbusdevice, modbus::ModbusRegisterType register_type, uint16_t start_address, uint16_t register_count,
      std::function<void(modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler);
  /** Create modbus read command
   *  Function code 02-04
   * @param modbusdevice pointer to the device to execute the command
   * @param function_code modbus function code for the read command
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_read_command(ModbusController *modbusdevice, modbus::ModbusRegisterType register_type,
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
  static ModbusCommandItem create_read_response(ModbusController *modbusdevice, uint16_t start_address, modbus::ModbusRegisterType register_type,
                                                                 const std::vector<uint16_t> &values);
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
      std::function<void(modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
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
      std::function<void(modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler = nullptr);

  bool is_equal(const ModbusCommandItem &other);
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
  ModbusController(uint16_t throttle = 0) : command_throttle_(throttle){};
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
  std::pair<uint16_t, uint16_t> get_message_info(const uint8_t* payload, size_t payload_len) override;
  /// default delegate called by process_modbus_data when a response has retrieved from the incoming queue
  void on_register_data(modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data);
  /// default delegate called by process_modbus_data when a response for a write response has retrieved from the
  /// incoming queue
  void on_write_register_response(modbus::ModbusRegisterType register_type, uint16_t start_address,
                                  const std::vector<uint8_t> &data);
  /// called by esphome generated code to set the command_throttle period
  void set_command_throttle(uint16_t command_throttle) { this->command_throttle_ = command_throttle; }

 protected:
  /// parse sensormap_ and create range of sequential addresses
  size_t create_register_ranges_();
  // find register in sensormap. Returns iterator with all registers having the same start address
  SensorSet find_sensors_(modbus::ModbusRegisterType register_type, uint16_t start_address) const;
  // find register in sensormap. Returns iterator with all registers having the start address
  SensorSet find_sensors_(modbus::ModbusRegisterType register_type, uint16_t start_address, uint16_t end_address) const;
  /// submit the read command for the address range to the send queue
  void update_range_(RegisterRange &r);
  /// parse incoming modbus data
  void process_modbus_data_(const ModbusCommandItem *response);
  /// send the next modbus command from the send queue
  bool send_next_command_();
  /// get the number of queued modbus commands (should be mostly empty)
  size_t get_command_queue_length_() { return command_queue_.size(); }
  /// dump the parsed sensormap for diagnostics
  void dump_sensors_();
  /// Collection of all sensors for this component
  SensorSet sensorset_;
  /// Continuous range of modbus registers
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

}  // namespace modbus_controller
}  // namespace esphome
