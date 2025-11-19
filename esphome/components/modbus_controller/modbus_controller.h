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
#include <memory>

namespace esphome {
namespace modbus_controller {

class ModbusController;

using modbus::ModbusFunctionCode;
using modbus::ModbusRegisterType;
using modbus::ModbusExceptionCode;
using namespace modbus::helpers;

class ModbusController;

class SensorItem {
 public:
  virtual void parse_and_publish(const std::vector<uint8_t> &data) = 0;
  virtual void on_write_response(const std::vector<uint8_t> &data);

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

class ModbusCommandItem;
struct RegisterRange {
  uint16_t start_address;
  ModbusRegisterType register_type;
  uint8_t register_count;
  uint16_t skip_updates;  // the config value
  SensorSet sensors;      // all sensors of this range
};

class ModbusCommandItem : public modbus::ModbusClientDevice {
 public:
  ModbusCommandItem(ModbusController *controller = nullptr);
  ModbusController *controller{nullptr};
  SensorSet sensors;  // all sensors of this range
  /// called when a modbus response was parsed without errors
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  /// called when a modbus error response was received
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
  /// called when modbus can't send for any reason
  void on_modbus_not_sent() override;
  /// called when a modbus timeout occurred
  void on_modbus_no_response() override;
  uint16_t register_address{0};
  uint16_t register_count{0};
  uint16_t skip_updates{0};
  ModbusFunctionCode function_code{ModbusFunctionCode::CUSTOM};
  ModbusRegisterType register_type{ModbusRegisterType::CUSTOM};
  std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
      on_data_func;
  std::vector<uint8_t> payload = {};
  bool send();

  /// factory methods
  /** Create modbus read command
   *  Function code 02-04
   * @param controller pointer to the controller
   * @param function_code modbus function code for the read command
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @param handler function called when the response is received
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_read_command(
      ModbusController *controller, ModbusRegisterType register_type, uint16_t start_address, uint16_t register_count,
      std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler = nullptr);

  /** Create modbus read command
   *  Function code 02-04
   * @param controller pointer to the controller
   * @param function_code modbus function code for the read command
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @param handler function called when the response is received
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_multiple_command(ModbusController *controller, uint16_t start_address,
                                                         uint16_t register_count, const std::vector<uint16_t> &values);
  /** Create modbus write multiple registers command
   *  Function 16 (10hex) Write Multiple Registers
   * @param controller pointer to the controller
   * @param start_address modbus address of the first register to read
   * @param register_count number of registers to read
   * @param value uint16_t single register value to write
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_single_command(ModbusController *controller, uint16_t start_address,
                                                       uint16_t value);
  /** Create modbus write single registers command
   *  Function 05 (05hex) Write Single Coil
   * @param controller pointer to the controller
   * @param start_address modbus address of the first register to read
   * @param value uint16_t data to be written to the registers
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_single_coil(ModbusController *controller, uint16_t address, bool value);

  /** Create modbus write multiple registers command
   *  Function 15 (0Fhex) Write Multiple Coils
   * @param controller pointer to the controller
   * @param start_address modbus address of the first register to read
   * @param value bool vector of values to be written to the registers
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_write_multiple_coils(ModbusController *controller, uint16_t start_address,
                                                       const std::vector<bool> &values);
  /** Create custom modbus command
   * @param controller pointer to the controller
   * @param values byte vector of data to be sent to the device. The complete payload must be provided with the
   * exception of the crc codes
   * @param handler function called when the response is received. Default is just logging a response
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_custom_command(
      ModbusController *controller, const std::vector<uint8_t> &values,
      std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler = nullptr);

  /** Create custom modbus command
   * @param controller pointer to the controller
   * @param values word vector of data to be sent to the device. The complete payload must be provided with the
   * exception of the crc codes
   * @param handler function called when the response is received. Default is just logging a response
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_custom_command(
      ModbusController *controller, const std::vector<uint16_t> &values,
      std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
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

class ModbusController : public PollingComponent, public modbus::ModbusClientDevice {
 public:
  void dump_config() override;
  void setup() override;
  void update() override;
  void set_online(bool online, const ModbusCommandItem &command_item);
  void increment_non_response_count();

  /// Registers a sensor with the controller. Called by esphomes code generator
  void add_sensor_item(SensorItem *item) { sensorset_.insert(item); }
  /// called by esphome generated code to set the offline_skip_updates
  void set_offline_skip_updates(uint16_t offline_skip_updates) { this->offline_skip_updates_ = offline_skip_updates; }
  /// get if the module is offline, didn't respond the last command
  bool get_module_offline() { return module_offline_; }
  /// Set callback for commands
  void add_on_command_sent_callback(std::function<void(int, int)> &&callback);
  /// Set callback for online changes
  void add_on_online_callback(std::function<void(int, int)> &&callback);
  /// Set callback for offline changes
  void add_on_offline_callback(std::function<void(int, int)> &&callback);
  /// called by esphome generated code to set the max_cmd_retries.
  void set_max_cmd_retries(uint8_t max_cmd_retries) { this->max_cmd_retries_ = max_cmd_retries; }
  /// get how many times a command will be (re)sent if no response is received
  uint8_t get_max_cmd_retries() { return this->max_cmd_retries_; }
  /// Check if the command should be retried based on the max_retries parameter
  bool can_send() { return this->cmd_non_responses_ <= this->max_cmd_retries_; };

  // Queue a one-shot command (will not be polled)
  void queue_command(const ModbusCommandItem &command);
  void unqueue_command(const ModbusCommandItem *command);

 protected:
  friend ModbusCommandItem;
  /// parse sensormap_ and create range of sequential addresses
  size_t create_register_ranges_();
  /// submit the read command for the address range to the send queue
  void update_range_(ModbusCommandItem &cmd);
  /// dump the parsed sensormap for diagnostics
  void dump_sensors_();
  /// Collection of all sensors for this component
  SensorSet sensorset_;
  /// Continuous range of modbus registers
  std::vector<ModbusCommandItem> polling_command_items_{};
  /// One-shot command items
  std::list<std::unique_ptr<ModbusCommandItem>> one_shot_command_items_;
  /// count updates to enable skipping
  uint16_t update_counter_{0};
  /// count updates to enable skipping
  uint16_t module_offline_at_{0};
  /// if module didn't respond the last command
  bool module_offline_{false};
  /// how many updates to skip if module is offline
  uint16_t offline_skip_updates_{0};
  /// How many times we will retry commands if we get no response
  uint8_t max_cmd_retries_{4};
  /// How many commands were sent without a response
  uint8_t cmd_non_responses_{0};
  /// Command sent callback
  CallbackManager<void(int, int)> command_sent_callback_{};
  /// Server online callback
  CallbackManager<void(int, int)> online_callback_{};
  /// Server offline callback
  CallbackManager<void(int, int)> offline_callback_{};
};

}  // namespace modbus_controller
}  // namespace esphome
