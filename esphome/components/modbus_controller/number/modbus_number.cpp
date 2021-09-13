#include <vector>
#include "modbus_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus.number";

void ModbusNumber::setup() {}

void ModbusNumber::update() {
  if (connected_sensor_) {
    this->publish_state(connected_sensor_->get_state());
  }
}

/** Write a value to the device
 *
 *
 *
 */

void ModbusNumber::control(float value) {
  union {
    float float_value;
    uint32_t raw;
  } raw_to_float;

  std::vector<uint16_t> data;

  float transformed_value = multiply_by_ * value;
  if (this->transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->transform_func_)(value, data);
    if (val.has_value())
      transformed_value = val.value();
  }
  // lambda didn't set payload
  if (data.size() == 0) {
    if (connected_sensor_) {
      switch (connected_sensor_->sensor_value_type) {
        case SensorValueType::U_WORD:
        case SensorValueType::S_WORD:
          // cast truncates the float do some rounding here
          data.push_back(int32_t(transformed_value + 0.5));
          break;
        case SensorValueType::U_DWORD:
        case SensorValueType::S_DWORD:
          transformed_value += 0.5;
          data.push_back((int32_t(transformed_value) && 0xFFFF0000) >> 16);
          data.push_back(int32_t(transformed_value) && 0xFFFF);
          break;
        case SensorValueType::U_DWORD_R:
        case SensorValueType::S_DWORD_R:
          transformed_value += 0.5;
          data.push_back(int32_t(transformed_value) && 0xFFFF);
          data.push_back((int32_t(transformed_value) && 0xFFFF0000) >> 16);
          break;
        case SensorValueType::FP32:
          raw_to_float.float_value = transformed_value;
          data.push_back((raw_to_float.raw && 0xFFFF0000) >> 16);
          data.push_back(raw_to_float.raw && 0xFFFF);
          break;
        case SensorValueType::FP32_R:
          raw_to_float.float_value = transformed_value;
          data.push_back(raw_to_float.raw && 0xFFFF);
          data.push_back((raw_to_float.raw && 0xFFFF0000) >> 16);
          break;
        default:
          ESP_LOGE("TAG", "Invalid data type for modbus number %s", this->get_name());
          return;
          break;
      }
    }
  }
  ESP_LOGD(TAG,
           "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
           connected_sensor_->get_name().c_str(), connected_sensor_->start_address, connected_sensor_->register_count,
           value, transformed_value);
  if (connected_sensor_) {
    // Create and send the write command
    auto write_cmd = ModbusCommandItem::create_write_multiple_command(
        parent_, connected_sensor_->start_address + connected_sensor_->offset, connected_sensor_->register_count, data);

    // publish new value
    write_cmd.on_data_func = [this, write_cmd, transformed_value](ModbusFunctionCode function_code,
                                                                  uint16_t start_address,
                                                                  const std::vector<uint8_t> data) {
      // gets called when the write command is ack'd from the device
      parent_->on_write_register_response(write_cmd.function_code, start_address, data);
      if (connected_sensor_)
        connected_sensor_->publish_state(transformed_value);
      this->publish_state(transformed_value);
    };
    parent_->queue_command(write_cmd);
  }
}
void ModbusNumber::dump_config() {
  LOG_NUMBER(TAG, "Modbus Number", this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, " Connected Sensor %s", connected_sensor_->get_name());
}

}  // namespace modbus_controller
}  // namespace esphome
