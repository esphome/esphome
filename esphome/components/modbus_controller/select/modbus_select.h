#pragma once

#include <utility>
#include <vector>

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modbus_controller {

class ModbusSelect : public Component, public select::Select, public SensorItem {
 public:
  ModbusSelect(SensorValueType sensor_value_type, uint16_t start_address, uint8_t register_count, uint16_t skip_updates,
               bool force_new_range, std::vector<int64_t> mapping) {
    this->register_type = ModbusRegisterType::HOLDING;  // not configurable
    this->sensor_value_type = sensor_value_type;
    this->start_address = start_address;
    this->offset = 0;            // not configurable
    this->bitmask = 0xFFFFFFFF;  // not configurable
    this->register_count = register_count;
    this->response_bytes = 0;  // not configurable
    this->skip_updates = skip_updates;
    this->force_new_range = force_new_range;
    this->mapping_ = std::move(mapping);
  }

  using transform_func_t =
      std::function<optional<std::string>(ModbusSelect *const, int64_t, const std::vector<uint8_t> &)>;
  using write_transform_func_t =
      std::function<optional<int64_t>(ModbusSelect *const, const std::string &, int64_t, std::vector<uint16_t> &)>;

  void set_parent(ModbusController *const parent) { this->parent_ = parent; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_template(transform_func_t &&f) { this->transform_func_ = f; }
  void set_write_template(write_transform_func_t &&f) { this->write_transform_func_ = f; }

  void dump_config() override;
  void parse_and_publish(const std::vector<uint8_t> &data) override;
  void control(const std::string &value) override;

 protected:
  std::vector<int64_t> mapping_{};
  ModbusController *parent_{nullptr};
  bool use_write_multiple_{false};
  bool optimistic_{false};
  optional<transform_func_t> transform_func_{nullopt};
  optional<write_transform_func_t> write_transform_func_{nullopt};
};

}  // namespace modbus_controller
}  // namespace esphome
