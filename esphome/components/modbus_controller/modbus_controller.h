#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "esphome/components/modbus/modbus.h"

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <list>
#include <atomic>

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
  READ_FIFO_QUEUE = 0x18                 // not implemented

};

enum class SensorValueType : uint8_t {
  RAW = 0x00,      // variable length
  U_WORD = 0x01,   // 1 Register unsigned
  U_DWORD = 0x02,  // 2 Registers unsigned
  S_WORD = 0x03,   // 1 Register signed
  S_DWORD = 0x04,  // 2 Registers signed
  BIT = 0x05,
  U_DWORD_R = 0x06,  // 2 Registers unsigned
  S_DWORD_R = 0x07,  // 2 Registers unsigned
  U_QWORD = 0x8,
  S_QWORD = 0x9,
  U_QWORD_R = 0xA,
  S_QWORD_R = 0xB
};

struct RegisterRange {
  uint16_t start_address;
  ModbusFunctionCode register_type;
  uint8_t register_count;
  uint8_t skip_updates;  // the config value
  uint64_t first_sensorkey;
  uint8_t skip_updates_counter;  // the running value
} __attribute__((packed));

// All sensors are stored in a map
// to enable binary sensors for values encoded as bits in the same register the key of each sensor
// the key is a 64 bit integer that combines the register properties
// sensormap_ is sorted by this key. The key ensures the correct order when creating consequtive ranges
// Format:  function_code (8 bit) | start address (16 bit)| offset (8bit)| bitmask (32 bit)
inline uint64_t calc_key(ModbusFunctionCode function_code, uint16_t start_address, uint8_t offset = 0,
                         uint32_t bitmask = 0) {
  return uint64_t((uint16_t(function_code) << 24) + (uint32_t(start_address) << 8) + (offset & 0xFF)) << 32 | bitmask;
}
inline uint16_t register_from_key(uint64_t key) { return (key >> 40) & 0xFFFF; }

// Get a byte from a hex string
//  hex_byte_from_str("1122",1) returns uint_8 value 0x22 == 34
//  hex_byte_from_str("1122",0) returns 0x11
//  position is the offset in bytes.
//  Because each byte is encoded in 2 hex digits
//  the position of the original byte in the hex string is byte_pos * 2
inline uint8_t c_to_hex(char c) { return (c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0'); }
inline uint8_t byte_from_hex_str(const std::string &value, uint8_t pos) {
  if (value.length() < pos * 2 + 1)
    return 0;
  return (c_to_hex(value[pos * 2]) << 4) | c_to_hex(value[pos * 2 + 1]);
}

inline uint16_t word_from_hex_str(const std::string &value, uint8_t pos) {
  return byte_from_hex_str(value, pos) << 8 | byte_from_hex_str(value, pos + 1);
}

inline uint32_t dword_from_hex_str(const std::string &value, uint8_t pos) {
  return word_from_hex_str(value, pos) << 16 | word_from_hex_str(value, pos + 2);
}

inline uint64_t qword_from_hex_str(const std::string &value, uint8_t pos) {
  return static_cast<uint64_t>(dword_from_hex_str(value, pos)) << 32 | dword_from_hex_str(value, pos + 4);
}

std::string get_hex_string(const std::vector<uint8_t> &data);

// Extract data from modbus response buffer
template<typename T> T get_data(const std::vector<uint8_t> &data, size_t offset) {
  if (sizeof(T) == sizeof(uint8_t)) {
    return T(data[offset]);
  }
  if (sizeof(T) == sizeof(uint16_t)) {
    return T((uint16_t(data[offset + 0]) << 8) | (uint16_t(data[offset + 1]) << 0));
  }

  if (sizeof(T) == sizeof(uint32_t)) {
    return get_data<uint16_t>(data, offset) << 16 | get_data<uint16_t>(data, (offset + 2));
  }

  if (sizeof(T) == sizeof(uint64_t)) {
    return static_cast<uint64_t>(get_data<uint32_t>(data, offset)) << 32 |
           (static_cast<uint64_t>(get_data<uint32_t>(data, offset + 4)));
  }
}

inline bool coil_from_vector(int coil, const std::vector<uint8_t> &data) {
  auto data_byte = coil / 8;
  return (data[data_byte] & (1 << (coil % 8))) > 0;
}

class ModbusController;

struct SensorItem {
  ModbusFunctionCode register_type;
  SensorValueType sensor_value_type;
  uint16_t start_address;
  uint32_t bitmask;
  uint8_t offset;
  uint8_t register_count;
  uint8_t skip_updates;

  virtual std::string const &get_sensorname() = 0;
  virtual void log() = 0;
  virtual float parse_and_publish(const std::vector<uint8_t> &data) = 0;

  uint64_t getkey() const { return calc_key(register_type, start_address, offset, bitmask); }
  size_t get_register_size() const {
    size_t size = 0;
    switch (sensor_value_type) {
      case SensorValueType::BIT:
        size = 1;
        break;
      case SensorValueType::U_WORD:
      case SensorValueType::S_WORD:
        size = 2;
        break;
      case SensorValueType::RAW:
      case SensorValueType::U_DWORD:
      case SensorValueType::S_DWORD:
      case SensorValueType::U_DWORD_R:
      case SensorValueType::S_DWORD_R:
        size = 4;
        break;
      case SensorValueType::U_QWORD:
      case SensorValueType::U_QWORD_R:
      case SensorValueType::S_QWORD:
      case SensorValueType::S_QWORD_R:
        size = 8;
        break;
    }
    return size;
  }
};

struct ModbusCommandItem {
  static const size_t MAX_PAYLOAD_BYTES = 240;
  ModbusController *modbusdevice;
  uint16_t register_address;
  uint16_t register_count;
  ModbusFunctionCode function_code;
  std::function<void(ModbusFunctionCode function_code, uint16_t start_address, const std::vector<uint8_t> &data)>
      on_data_func;
  std::vector<uint8_t> payload = {};
  bool send();
  // factory methods
  static ModbusCommandItem create_read_command(
      ModbusController *modbusdevice, ModbusFunctionCode function_code, uint16_t start_address, uint16_t register_count,
      std::function<void(ModbusFunctionCode function_code, uint16_t start_address, const std::vector<uint8_t> &data)>
          &&handler);

  static ModbusCommandItem create_read_command(ModbusController *modbusdevice, ModbusFunctionCode function_code,
                                               uint16_t start_address, uint16_t register_count);
  static ModbusCommandItem create_write_multiple_command(ModbusController *modbusdevice, uint16_t start_address,
                                                         uint16_t register_count, const std::vector<uint16_t> &values);
  static ModbusCommandItem create_write_single_command(ModbusController *modbusdevice, uint16_t start_address,
                                                       int16_t value);
  static ModbusCommandItem create_write_single_coil(ModbusController *modbusdevice, uint16_t address, bool value);
  static ModbusCommandItem create_write_multiple_coils(ModbusController *modbusdevice, uint16_t start_address,
                                                       const std::vector<bool> &values);
  static ModbusCommandItem create_custom_command(ModbusController *modbusdevice, const std::vector<uint8_t> &values);
};

class ModbusController : public PollingComponent, public modbus::ModbusDevice {
 public:
  ModbusController(uint16_t throttle = 0) : modbus::ModbusDevice(), command_throttle_(throttle){};
  size_t create_register_ranges();
  void update() override;
  void update_range(RegisterRange &r);
  void setup() override;
  void loop() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
  void process_modbus_data(const ModbusCommandItem *response);

  void dump_config() override;
  void on_write_register_response(ModbusFunctionCode function_code, uint16_t start_address,
                                  const std::vector<uint8_t> &data);
  void on_register_data(ModbusFunctionCode function_code, uint16_t start_address, const std::vector<uint8_t> &data);
  void set_command_throttle(uint16_t command_throttle) { this->command_throttle_ = command_throttle; }

  void queue_command(const ModbusCommandItem &command);
  void add_sensor_item(SensorItem *item) { sensormap_[item->getkey()] = item; }
  size_t get_command_queue_length() { return command_queue_.size(); }
  void set_ctrl_pin(uint8_t ctrl_pin) {
    static GPIOPin PIN(ctrl_pin, OUTPUT);

    parent_->set_flow_control_pin(&PIN);
  }

 protected:
  bool send_next_command_();
  // Collection of all sensors for this component
  // see calc_key how the key is contructed
  std::map<uint64_t, SensorItem *> sensormap_;
  // Continous range of modbus registers
  std::vector<RegisterRange> register_ranges_;
  // Hold the pending requests to be sent
  std::list<std::unique_ptr<ModbusCommandItem>> command_queue_;
  std::queue<std::unique_ptr<ModbusCommandItem>> incoming_queue_;
  uint32_t last_command_timestamp_;
  uint16_t command_throttle_;
};

}  // namespace modbus_controller
}  // namespace esphome
