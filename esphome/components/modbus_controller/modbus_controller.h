#pragma once

#include "esphome/core/component.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/automation.h"

#include <list>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace esphome::modbus_controller {

class ModbusController;

using modbus::EntityType;
using modbus::ExceptionCode;
using modbus::FunctionCode;
using modbus::helpers::SensorValueType;

// Remove before 2027.2.0 - deprecated names re-exported so external components keep their warning window
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
using modbus::ModbusExceptionCode;
using modbus::ModbusFunctionCode;
using modbus::ModbusRegisterType;
#pragma GCC diagnostic pop

// Remove before 2026.10.0 — these helpers have moved to modbus::helpers
ESPDEPRECATED("Use modbus::helpers::value_type_is_float() instead. Removed in 2026.10.0", "2026.4.0")
inline bool value_type_is_float(SensorValueType v) { return modbus::helpers::value_type_is_float(v); }

ESPDEPRECATED("Use modbus::helpers::modbus_register_read_function() instead. Removed in 2026.10.0", "2026.4.0")
inline FunctionCode modbus_register_read_function(modbus::EntityType reg_type) {
  return modbus::helpers::modbus_register_read_function(reg_type);
}

ESPDEPRECATED("Use modbus::helpers::modbus_register_write_function() instead. Removed in 2026.10.0", "2026.4.0")
inline FunctionCode modbus_register_write_function(modbus::EntityType reg_type) {
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

// Span overloads of the deprecated helpers below: read lambdas receive their payload as a
// std::span<const uint8_t> (previously a const std::vector<uint8_t> &), and a span does not convert to
// a vector, so existing lambdas calling these by name need an overload that accepts one. These carry
// this release's deprecation window, since the span forms only exist from it.
// payload_to_number() deliberately has no such overload: one of its arguments is a modbus::helpers
// type, so a span call already reaches the helper by argument-dependent lookup, and a forwarder here
// would only make that call ambiguous.
// Remove before 2027.2.0.
template<typename T>
ESPDEPRECATED("Use modbus::helpers::get_data() instead. Removed in 2027.2.0", "2026.8.0")
T get_data(std::span<const uint8_t> data, size_t buffer_offset) {
  return modbus::helpers::get_data<T>(data.data(), buffer_offset);
}

// Remove before 2027.2.0 (window restarted when the migration target changed to bit_from_packed())
ESPDEPRECATED("Use modbus::helpers::bit_from_packed() instead. Removed in 2027.2.0", "2026.4.0")
inline bool coil_from_vector(int coil, const std::vector<uint8_t> &data) {
  return modbus::helpers::bit_from_packed(coil, data);
}

// Remove before 2027.2.0
ESPDEPRECATED("Use modbus::helpers::bit_from_packed() instead. Removed in 2027.2.0", "2026.8.0")
inline bool coil_from_vector(int coil, std::span<const uint8_t> data) {
  return modbus::helpers::bit_from_packed(coil, data);
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
  return modbus::helpers::payload_to_number(std::span<const uint8_t>(data), sensor_value_type, offset, bitmask)
      .value_or(0);
}

ESPDEPRECATED("Use modbus::helpers::float_to_payload() instead. Removed in 2026.10.0", "2026.4.0")
inline std::vector<uint16_t> float_to_payload(float value, SensorValueType value_type) {
  std::vector<uint16_t> data;
  modbus::helpers::float_to_payload(data, value, value_type);
  return data;
}

class ModbusController;

class SensorItem {
 public:
  /// Parse this sensor's slice out of its range's response and publish it. The span points into the
  /// response buffer and is only valid for the duration of the call. Read the sensor's data from
  /// `offset` within it.
  virtual void parse_and_publish(std::span<const uint8_t> data) = 0;

  /// Coils and discrete inputs address individual bits; every other type addresses 16-bit registers.
  bool addresses_bits() const { return modbus::helpers::is_entity_type_binary(this->register_type); }

  /// Address a write entity (switch/number/select) targets, derived from its resolved position within
  /// the range so that a write lands on the register the sensor reads from.
  uint16_t write_address() const {
    return this->range_start_address + (this->addresses_bits() ? this->offset : this->offset / 2);
  }

  /// Records the offset as configured, and seeds the resolved position with it. Building the ranges
  /// overwrites `offset` with the position within the range; an item that is never polled keeps this
  /// value, which is what its own address arithmetic expects.
  void set_offset_from_start_address(uint8_t offset) {
    this->offset_from_start_address = offset;
    this->offset = offset;
  }

  /// Sets the configured address, and points the range base at it. Building the ranges moves the base
  /// to the range's first register; an item that is never polled (an output, or a switch with
  /// assumed_state) keeps its own address, so write_address() stays correct for it.
  void set_address(uint16_t address) {
    this->start_address = address;
    this->range_start_address = address;
  }

  void set_custom_data(const std::vector<uint8_t> &data) { custom_data = data; }
  size_t virtual get_register_size() const {
    if (this->addresses_bits()) {
      return 1;
    } else {  // if CONF_RESPONSE_BYTES is used override the default
      return response_bytes > 0 ? response_bytes : register_count * 2;
    }
  }
  // Override register size for modbus devices not using 1 register for one dword
  void set_register_size(uint8_t register_size) { response_bytes = register_size; }
  modbus::EntityType register_type{modbus::EntityType::CUSTOM};
  SensorValueType sensor_value_type{SensorValueType::RAW};
  uint16_t start_address{0};
  uint32_t bitmask{0};
  /// Position of this sensor's data within its range's response - a byte offset for registers, a bit
  /// index for coils and discrete inputs. Resolved while the ranges are built, so it already accounts
  /// for the registers ahead of it (including wide response_size ones) and for any offset inherited
  /// from an earlier sensor sharing the same register.
  uint8_t offset{0};
  uint8_t register_count{0};
  uint8_t response_bytes{0};
  /// The offset exactly as configured: measured from this sensor's own start_address, where `offset`
  /// is measured from the first register of the range it ends up polled in. Same units as `offset` -
  /// bytes for registers, bits for coils and discrete inputs. Kept so the resolution can be recomputed,
  /// and so the sort order of the sensor set never depends on the resolved value.
  /// Declared before range_start_address so it lands in the padding after response_bytes.
  uint8_t offset_from_start_address{0};
  /// First register of the range this sensor is polled in; equals start_address for an unpolled item.
  uint16_t range_start_address{0};
  uint16_t skip_updates{0};
  std::vector<uint8_t> custom_data{};
  bool force_new_range{false};
};

// ModbusController::create_polling_commands_ tries to optimize register range
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

    // sort by the offset as configured (ensures update of sensors in ascending order). The resolved
    // `offset` is deliberately not used: ranges are built while iterating this set and assign it, and
    // a sort key that changed under the iteration would corrupt the set's ordering.
    if (lhs->offset_from_start_address != rhs->offset_from_start_address) {
      return lhs->offset_from_start_address < rhs->offset_from_start_address;
    }

    // The pointer to the sensor is used last to ensure that
    // multiple sensors with the same values can be added with a stable sort order.
    return lhs < rhs;
  }
};

using SensorSet = std::set<SensorItem *, SensorItemsComparator>;

struct RegisterRange {
  uint16_t start_address;
  modbus::EntityType register_type;
  uint8_t register_count;
  uint16_t skip_updates;  // the config value
  SensorSet sensors;      // all sensors of this range
};

/// A single modbus command. Each command is its own ModbusClientDevice: it sends its frame to the hub
/// and the hub routes the response back to this object's on_modbus_* callbacks, so the controller no
/// longer has to match responses to a FIFO queue.
class ModbusCommandItem : public modbus::ModbusClientDevice {
 public:
  /// Empty command with no controller connection (kept for source compatibility with value-type usage).
  ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address)
      : modbus::ModbusClientDevice(parent, address), controller_(&controller) {}
  /// Read command built from a range; the read PDU is rebuilt from these fields at send time.
  ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address,
                    RegisterRange &&range);
  /// Custom polling command: the PDU bytes are referenced from the sensor (not copied); responses are
  /// dispatched to that sensor.
  ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address, SensorItem *sensor);

  // The base deletes copy/move (its destructor unregisters the device from the hub queue), but command
  // items are stored in value containers, so copy/move CONSTRUCTION is re-provided (copy only for the
  // queue_command() path). Assignment stays deleted: the item's address-in-memory is its hub identity.
  ModbusCommandItem(const ModbusCommandItem &other);
  ModbusCommandItem(ModbusCommandItem &&other) noexcept;
  ModbusCommandItem &operator=(ModbusCommandItem &&) = delete;

  SensorSet sensors;  // sensors served by this command (empty for factory/write commands)
  uint16_t skip_updates{0};
  std::function<void(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data)> on_data_func;
  /// Write data bytes for the command (register/coil values), or the raw frame of a one-shot custom
  /// command; reads leave it empty. Small-buffer optimized: fixed-size commands (single-register/coil
  /// writes) fit in the 8-byte inline buffer with no heap; only large multi-register or custom frames
  /// spill to a single one-time heap allocation. This keeps runtime one-shot writes off the heap without
  /// reserving a max-size buffer per command item.
  SmallInlineBuffer<8> payload;
  // Set by unqueue_command() when this one-shot has completed. The controller erases flagged items at a
  // safe point (update()/queue_command()), never from inside the command's own callback.
  bool pending_removal{false};

  /// called when a modbus response was parsed without errors
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override;
  /// called when a modbus error (exception) response was received
  void on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) override;
  /// called when the command could not be sent
  void on_not_sent(std::span<const uint8_t> request_pdu) override;
  /// called when the command's frame is actually written to the wire; fires the on_command_sent trigger
  void on_sent(std::span<const uint8_t> request_pdu) override;
  /// called on timeout; returns true to have the hub re-queue the frame for a retry
  bool on_no_response(std::span<const uint8_t> request_pdu) override;

  uint16_t register_address() const { return this->start_address_; }
  uint16_t register_count() const { return this->register_count_; }
  EntityType register_type() const { return this->register_type_; }

  /// Queue this command's frame on the hub. Returns false when refused, in which case no callback ever comes.
  /// The item is the hub device, so it must stay alive until its terminal callback; a destroyed item's
  /// pending frame is silently retired.
  bool send();

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
      ModbusController *modbusdevice, EntityType register_type, uint16_t start_address, uint16_t register_count,
      std::function<void(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data)> &&handler);
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
      std::function<void(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data)> &&handler =
          nullptr);

  /** Create custom modbus command
   * @param modbusdevice pointer to the device to execute the command
   * @param values word vector of data to be sent to the device. The complete payload must be provided with the
   * exception of the crc codes
   * @param handler function called when the response is received. Default is just logging a response
   * @return ModbusCommandItem with the prepared command
   */
  static ModbusCommandItem create_custom_command(
      ModbusController *modbusdevice, const std::vector<uint16_t> &values,
      std::function<void(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data)> &&handler =
          nullptr);

 protected:
  void set_command_(FunctionCode function_code, EntityType register_type, uint16_t start_address,
                    uint16_t register_count) {
    this->function_code_ = function_code;
    this->register_type_ = register_type;
    this->start_address_ = start_address;
    this->register_count_ = register_count;
  }
  EntityType register_type_{EntityType::CUSTOM};
  uint16_t start_address_{0};
  uint16_t register_count_{0};
  FunctionCode function_code_{FunctionCode::CUSTOM};
  /// Custom polling commands reference the PDU bytes owned by their SensorItem instead of copying them.
  const std::vector<uint8_t> *custom_data_{nullptr};
  ModbusController *controller_{nullptr};
};

/// Whether an offline probe is due this update cycle: every offline_skip_updates + 1 cycles,
/// anchored at the cycle the device went offline. Pure so the cadence (including update_counter
/// wraparound) can be unit tested; used by ModbusController::update().
inline bool offline_retry_due(uint16_t update_counter, uint16_t module_offline_at, uint16_t offline_skip_updates) {
  return static_cast<uint16_t>(update_counter + 1 - module_offline_at) % (offline_skip_updates + 1) == 0;
}

/** Modbus controller class.
 *   Each instance handles the modbus commuinication for all sensors with the same modbus address
 *
 * all sensor items (sensors, switches, binarysensor ...) are parsed in modbus address ranges.
 * when esphome calls ModbusController::Update the commands for each range are created and sent
 * Responses for the commands are dispatched to the modbus sensor items.
 */

class ModbusController final : public PollingComponent {
 public:
  void dump_config() override;
  // No loop() override: the hub owns transmit/receive timing and each command routes its own
  // response, so the controller never joins the looping components at all.
  void setup() override;
  void update() override;

  // The controller is not itself a modbus device - its commands and writer entities send as their own
  // devices. It only owns the hub + address so those senders can be built against them.
  void set_parent(modbus::ModbusClientHub *hub) { this->hub_ = hub; }
  void set_address(uint8_t address) { this->address_ = address; }

  /// The hub and modbus address this controller talks to. Used to build commands/entities that send as
  /// their own device.
  modbus::ModbusClientHub *hub() const { return this->hub_; }
  uint8_t device_address() const { return this->address_; }

  /// Queues a one-shot modbus command (writes, custom commands); taken by value, so std::move to avoid a copy.
  void queue_command(ModbusCommandItem command);
  /// Flags a finished one-shot command for removal. Called by the command as the last action of its own
  /// callback, so the item is not destroyed here (send() and the hub still touch it) but swept later.
  void unqueue_command(const ModbusCommandItem *command);
  /// Registers a sensor with the controller. Called by esphomes code generator
  void add_sensor_item(SensorItem *item) { sensorset_.insert(item); }
  /// Handles a write command acknowledgement (used by write command on_data_func handlers).
  void on_write_register_response(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data);
  /// Update the online/offline state after a response or a run of timeouts, firing the callbacks.
  void set_online(bool online, int function_code, int register_address);
  /// Fire the on_command_sent trigger (called when a command's frame reaches the wire).
  void command_sent(int function_code, int register_address) {
    this->command_sent_callback_.call(function_code, register_address);
  }
  /// A command timed out; bump the consecutive-timeout counter used by can_send()/offline detection.
  void increment_non_response_count() { this->cmd_non_responses_++; }
  /// Whether more retries are allowed before the device is considered offline. Deliberately pooled
  /// per device, not per command: online/offline is a property of the physical device.
  bool can_send() { return this->cmd_non_responses_ <= this->max_cmd_retries_; }
  /// called by esphome generated code to set the offline_skip_updates
  void set_offline_skip_updates(uint16_t offline_skip_updates) { this->offline_skip_updates_ = offline_skip_updates; }
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
  /// Group the registered sensors into contiguous ranges and create one polling command per range.
  void create_polling_commands_();
  /// build one persistent polling command from a range and add it to polling_command_items_
  void create_polling_command_(RegisterRange &&range) {
    // A custom range polls the first sensor's custom_data (a ready-made raw frame); it needs the
    // sensor constructor so the command references those bytes and decodes the real function code.
    // The response still dispatches to every sensor in the range.
    if (range.register_type == EntityType::CUSTOM && !range.sensors.empty()) {
      auto &cmd = this->polling_command_items_.emplace_back(*this, this->hub_, this->address_, *range.sensors.begin());
      cmd.sensors = std::move(range.sensors);
      cmd.skip_updates = range.skip_updates;  // the range's merged rate, not the first sensor's
    } else {
      this->polling_command_items_.emplace_back(*this, this->hub_, this->address_, std::move(range));
    }
  }
  /// send a range's polling command if it is due this update
  void update_range_(ModbusCommandItem &cmd);
  /// The hub this controller's commands/entities send through, and the modbus address they target.
  modbus::ModbusClientHub *hub_{nullptr};
  uint8_t address_{0};
  /// Collection of all sensors for this component
  SensorSet sensorset_;
  /// One persistent command per register range, each its own ModbusClientDevice. Built once in setup()
  /// (create_polling_commands_ feeds each range straight in; the vector may reallocate as it grows, which
  /// is safe because no command has registered with the hub yet) and never appended to afterward, so the
  /// hub's device pointers stay valid once commands start sending.
  std::vector<ModbusCommandItem> polling_command_items_{};
  /// Dynamically queued one-shot commands (writes, custom commands). std::list keeps stable addresses.
  std::list<std::unique_ptr<ModbusCommandItem>> one_shot_command_items_;
  /// Erases one-shot commands flagged by unqueue_command(). Safe even when reached from inside a hub
  /// callback (via an on_online/on_offline/on_command_sent automation that queues a command): the
  /// destructor detaches via clear_tx_queue_for_device(), which the hub allows from callbacks, and the
  /// item running its callback is not flagged until that callback returns.
  void sweep_completed_one_shots_();
  /// if module didn't respond the last command
  bool module_offline_{false};
  /// update_counter_ value at which the module went offline (for offline_skip_updates timing)
  uint16_t module_offline_at_{0};
  /// counts update() cycles; drives skip_updates and offline timing
  uint16_t update_counter_{0};
  /// consecutive non-responses; drives can_send() and offline detection
  uint8_t cmd_non_responses_{0};
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
inline float payload_to_float(std::span<const uint8_t> data, const SensorItem &item, uint8_t offset) {
  int64_t number = modbus::helpers::payload_to_number(data, item.sensor_value_type, offset, item.bitmask).value_or(0);

  float float_value;
  if (modbus::helpers::value_type_is_float(item.sensor_value_type)) {
    float_value = bit_cast<float>(static_cast<uint32_t>(number));
  } else {
    float_value = static_cast<float>(number);
  }

  return float_value;
}

// Remove before 2027.2.0 (window opened when this helper gained an explicit offset). item.offset is
// the item's resolved position within its range's response, so this decodes the same bytes as passing
// that offset explicitly.
ESPDEPRECATED("Pass the offset explicitly: payload_to_float(data, item, item.offset). Removed in 2027.2.0", "2026.8.0")
inline float payload_to_float(std::span<const uint8_t> data, const SensorItem &item) {
  return payload_to_float(data, item, item.offset);
}

}  // namespace esphome::modbus_controller
