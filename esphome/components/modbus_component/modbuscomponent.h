#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/modbus/modbus.h"
#include <queue>
#include <map>
#include <memory>

namespace esphome {
namespace modbus_component {

using namespace sensor;
struct ModbusCommandItem;
class ModbusComponent;

enum class ModbusFunctionCode {
  READ_COILS = 0x01,
  READ_DISCRETE_INPUTS = 0x02,
  READ_HOLDING_REGISTERS = 0x03,
  READ_INPUT_REGISTERS = 0x04,
  WRITE_SINGLE_COIL = 0x05,
  WRITE_SINGLE_REGISTER = 0x6,
  WRITE_MULTIPLE_REGISTERS = 0x10,
};

enum class SensorValueType : uint8_t {
  RAW = 0x00,       // variable length
  U_SINGLE = 0x01,  // 1 Register unsigned
  U_DOUBLE = 0x02,  // 2 Registers unsigned
  S_SINGLE = 0x03,  // 1 Register signed
  S_DOUBLE = 0x04,  // 2 Registers signed
  BIT = 0x05,
  U_DOUBLE_HILO = 0x06,  // 2 Registers unsigned
  S_DOUBLE_HILO = 0x07,  // 2 Registers unsigned    
};

struct RegisterRange {
  uint16_t start_address;
  uint8_t register_count;
  ModbusFunctionCode register_type;
  uint32_t first_sensorkey;
};

// All sensors are stored in a map
// to enable binary sensors for values encoded as bits in the same register the key of each sensor
// is start address +  off << 16 | bitpos

inline uint32_t calc_key(uint16_t start_address, uint8_t offset = 0, uint16_t bitmask = 0) {
  return (start_address + offset) << 16 | bitmask;
}
inline uint16_t register_from_key(uint32_t key) { return key >> 16; }

const std::function<float(int64_t)> DIVIDE_BY_100 = [](int64_t val) { return val / 100.0; };

struct SensorItem {
  ModbusFunctionCode register_type;
  uint16_t start_address;
  uint8_t offset;
  uint16_t bitmask;
  SensorValueType sensor_value_type;
  int64_t last_value;
  std::function<float(int64_t)> transform_expression;

  virtual std::string const &get_name() = 0;
  virtual void log() = 0;  // {}
  virtual float parse_and_publish(const std::vector<uint8_t> &data) = 0;
  uint32_t getkey() const { return calc_key(start_address, offset, bitmask); }
  size_t get_register_size() {
    size_t size = 0;
    switch (sensor_value_type) {
      case SensorValueType::RAW:
        size = 4;
        break;
      case SensorValueType::U_SINGLE:
        size = 1;
        break;
      case SensorValueType::U_DOUBLE:
        size = 1;
        break;
      case SensorValueType::S_SINGLE:
        size = 1;
        break;
      case SensorValueType::S_DOUBLE:
        size = 2;
        break;
      case SensorValueType::BIT:
        size = 1;
        break;
      case SensorValueType::U_DOUBLE_HILO:
        size = 2 ;
        break;
      case SensorValueType::S_DOUBLE_HILO:
        size = 2 ;
        break;        
      default:
        size = 1;
        break;
    }
    return size;
  }
};

struct FloatSensorItem : public SensorItem {
  sensor::Sensor *sensor_;
  FloatSensorItem(sensor::Sensor *sensor) : sensor_(sensor) {}
  std::string const &get_name() override { return sensor_->get_name(); }
  void log() override;
  float parse_and_publish(const std::vector<uint8_t> &data) override;
};

struct BinarySensorItem : public SensorItem {
  binary_sensor::BinarySensor *sensor_;
  BinarySensorItem(binary_sensor::BinarySensor *sensor) : sensor_(sensor) {}
  std::string const &get_name() override { return sensor_->get_name(); }
  void log() override;
  float parse_and_publish(const std::vector<uint8_t> &data) override;
};

// class ModbusSensor ;
class ModbusComponent : public PollingComponent, public modbus::ModbusDevice {
 public:
  std::map<uint32_t, std::unique_ptr<SensorItem>> sensormap;
  std::vector<RegisterRange> register_ranges;
  void add_sensor(sensor::Sensor *sensor, ModbusFunctionCode register_type, uint16_t start_address, uint8_t offset,
                  uint16_t bitmask, SensorValueType value_type = SensorValueType::U_SINGLE, float scale_factor = 0.01) {
    auto new_item = make_unique<FloatSensorItem>(sensor);
    new_item->register_type = register_type;
    new_item->start_address = start_address;
    new_item->offset = offset;
    new_item->bitmask = bitmask;
    new_item->sensor_value_type = value_type;
    // used to cache prev. value.
    // because values are  only in the int32_t range it's a safe "marker" value
    new_item->last_value = INT64_MIN;
    // Default transformation is divide by 100
    new_item->transform_expression = [scale_factor](int64_t val) { return val * scale_factor; };
    sensormap[new_item->getkey()] = std::move(new_item);
  }
  void add_binarysensor(binary_sensor::BinarySensor *sensor, ModbusFunctionCode register_type, uint16_t start_address,
                        uint8_t offset, uint16_t bitmask) {
    auto new_item = make_unique<BinarySensorItem>(sensor);
    new_item->register_type = register_type;
    new_item->start_address = start_address;
    new_item->offset = offset;
    new_item->bitmask = bitmask;
    new_item->sensor_value_type = SensorValueType::BIT;
    new_item->last_value = INT64_MIN;
    // not sure we need it anymore
    new_item->transform_expression = [bitmask](int64_t val) { return (val & bitmask & 0x0000FFFFF) ? 1 : 0; };
    auto key = new_item->getkey();
    sensormap[key] = std::move(new_item);
  }

  size_t create_register_ranges();

  bool remove_register_range(uint16_t start_address) {
    bool found = false;

    for (auto it = this->register_ranges.begin(); it != this->register_ranges.end();) {
      if (it->start_address == start_address) {
        // First delete sensors from the map
        auto first_sensor = sensormap.find(it->first_sensorkey);
        if (first_sensor != sensormap.end()) {
          // loop through all sensors with the same start address
          auto last_sensor = first_sensor;
          while (last_sensor != sensormap.end() && last_sensor->second->start_address == start_address) {
            last_sensor++;
          }
          sensormap.erase(first_sensor, last_sensor);
        }
        // Remove the range itself
        it = this->register_ranges.erase(it);
        found = true;
      } else {
        it++;
      }
    }
    return found;
  }

  // find a register by it's address regardless of the offset
  SensorItem *find_by_register_address(uint16_t register_address) {
    // Not extemly effienct but the number of registers isn't that large
    // and the operation is used only in special cases
    // like changing a property during setup
    for (auto &item : this->sensormap) {
      if (register_address == item.second->start_address + item.second->offset) {
        return item.second.get();
      }
    }
    return nullptr;
  }

  void update() override;
  void setup() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;

  void dump_config() override;
  // set the RTC Clock of the controller to current time. Note: make sure to add sntp config
  void set_realtime_clock_to_now();

  void on_write_register_response(uint16_t start_address, const std::vector<uint8_t> &data);
  void on_register_data(uint16_t start_address, const std::vector<uint8_t> &data);

 protected:
  // Hold the pending requests to sent
  std::queue<std::unique_ptr<ModbusCommandItem>> command_queue_;

  void queue_command_(const ModbusCommandItem &command) {
    command_queue_.push(make_unique<ModbusCommandItem>(command));
  }

  bool send_next_command_();
};

struct ModbusCommandItem {
  // keep memory consumption low.  Since all registers are 2 bytes and only write RTC needs to be written in 1 command 8
  // bytes is enough
  static const size_t MAX_PAYLOAD_BYTES = 240;
  ModbusComponent *modbusdevice;
  uint16_t register_address;
  uint16_t register_count;
  uint16_t expected_response_size;
  ModbusFunctionCode function_code;
  std::function<void(uint16_t start_address, const std::vector<uint8_t> &data)> on_data_func;
  std::vector<uint16_t> payload = {};
  bool send();

  // factory methods
  static ModbusCommandItem create_read_command(
      ModbusComponent *modbusdevice, ModbusFunctionCode function_code, uint16_t start_address, uint16_t register_count,
      std::function<void(uint16_t start_address, const std::vector<uint8_t> &data)> &&handler) {
    ModbusCommandItem cmd;
    cmd.modbusdevice = modbusdevice;
    cmd.function_code = function_code;
    cmd.register_address = start_address;
    cmd.expected_response_size = register_count * 2;
    cmd.register_count = register_count;
    cmd.on_data_func = std::move(handler);
    // adjust expected response size for ReadCoils and DiscretInput
    if (cmd.function_code == ModbusFunctionCode::READ_COILS) {
      cmd.expected_response_size = (register_count + 7) / 8;
    }
    if (cmd.function_code == ModbusFunctionCode::READ_DISCRETE_INPUTS) {
      cmd.expected_response_size = 1;
    }
    return cmd;
  }

  static ModbusCommandItem create_read_command(ModbusComponent *modbusdevice, ModbusFunctionCode function_code,
                                               uint16_t start_address, uint16_t register_count) {
    ModbusCommandItem cmd;
    cmd.modbusdevice = modbusdevice;
    cmd.function_code = function_code;
    cmd.register_address = start_address;
    cmd.expected_response_size = register_count * 2;
    cmd.register_count = register_count;
    cmd.on_data_func = [modbusdevice](uint16_t start_address, const std::vector<uint8_t> data) {
      modbusdevice->on_register_data(start_address, data);
    };
    // adjust expected response size for ReadCoils and DiscretInput
    if (cmd.function_code == ModbusFunctionCode::READ_COILS) {
      cmd.expected_response_size = (register_count + 7) / 8;
    }
    if (cmd.function_code == ModbusFunctionCode::READ_DISCRETE_INPUTS) {
      cmd.expected_response_size = 1;
    }
    return cmd;
  }

  static ModbusCommandItem create_write_multiple_command(ModbusComponent *modbusdevice, uint16_t start_address,
                                                         uint16_t register_count, const std::vector<uint16_t> &values);
  static ModbusCommandItem create_write_single_command(ModbusComponent *modbusdevice, uint16_t start_address,
                                                       int16_t value);
};

}  // namespace modbus_sensor
}  // namespace esphome
