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

 */

void ModbusNumber::control(float value) {
  union {
    float float_value;
    uint32_t raw;
  } raw_to_float;

  std::vector<uint16_t> data;
  auto original_value = value;
  // Is there are lambda configured?
  if (this->transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->transform_func_)(value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      value = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  } else {
    value = multiply_by_ * value;
  }

  if (connected_sensor_) {
    // lambda didn't set payload
    if (data.empty()) {
      data = float_to_payload(value, connected_sensor_->sensor_value_type);
    }

    ESP_LOGD(TAG,
             "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
             connected_sensor_->get_name().c_str(), connected_sensor_->start_address, connected_sensor_->register_count,
             value, value);

    // Create and send the write command
    auto write_cmd = ModbusCommandItem::create_write_multiple_command(
        parent_, connected_sensor_->start_address + connected_sensor_->offset, connected_sensor_->register_count, data);

    // publish new value
    write_cmd.on_data_func = [this, write_cmd, value](ModbusFunctionCode function_code, uint16_t start_address,
                                                      const std::vector<uint8_t> &data) {
      // gets called when the write command is ack'd from the device
      parent_->on_write_register_response(write_cmd.function_code, start_address, data);
      if (connected_sensor_)
        connected_sensor_->publish_state(value);
      this->publish_state(value);
    };
    parent_->queue_command(write_cmd);
  }
}
void ModbusNumber::dump_config() {
  LOG_NUMBER(TAG, "Modbus Number", this);
  ESP_LOGCONFIG(TAG, " Connected Sensor %s", connected_sensor_->get_name().c_str());
}

}  // namespace modbus_controller
}  // namespace esphome
