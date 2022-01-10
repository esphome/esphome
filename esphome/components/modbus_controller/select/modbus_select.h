#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modbus_controller {

class ModbusSelect : public Component, public select::Select, public SensorItem {
 public:
  ModbusSelect(uint16_t start_address, uint8_t register_count, uint8_t skip_updates, bool force_new_range,
               std::vector<uint64_t> mapping)
      : Component(), select::Select(), write_address(start_address) {
    this->register_type = ModbusRegisterType::HOLDING;  // not configurable
    this->start_address = start_address;
    this->offset = 0;                                // not configurable
    this->bitmask = 0xFFFFFFFF;                      // not configurable
    this->sensor_value_type = SensorValueType::RAW;  // not configurable
    this->register_count = register_count;
    this->response_bytes = 0;  // not configurable
    this->skip_updates = skip_updates;
    this->force_new_range = force_new_range;
    this->mapping = mapping;
  }

  void set_parent(ModbusController *const parent) { this->parent_ = parent; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

  void dump_config() override;
  void parse_and_publish(const std::vector<uint8_t> &data) override;
  void control(const std::string &value) override;

 protected:
  const uint16_t write_address;
  std::vector<uint64_t> mapping;
  ModbusController *parent_;
  bool use_write_multiple_{false};
};

}  // namespace modbus_controller
}  // namespace esphome
