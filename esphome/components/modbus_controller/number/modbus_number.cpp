#include <vector>
#include "modbus_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus.number";

void ModbusNumber::parse_and_publish(const std::vector<uint8_t> &data) {
  float result = payload_to_float(data, *this) / this->multiply_by_;

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->transform_func_)(this, result, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      result = val.value();
    }
  }
  ESP_LOGD(TAG, "Number new state : %.02f", result);
  // this->sensor_->raw_state = result;
  this->publish_state(result);
}

void ModbusNumber::control(float value) {
  ModbusCommandItem write_cmd;
  std::vector<uint16_t> data;
  float write_value = value;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      write_value = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  } else {
    write_value = this->multiply_by_ * write_value;
  }

  if (!data.empty()) {
    ESP_LOGV(TAG, "Modbus Number write raw: %s", format_hex_pretty(data).c_str());
    write_cmd = ModbusCommandItem::create_custom_command(
        this->parent_, data,
        [this, write_cmd](ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
          this->parent_->on_write_register_response(write_cmd.register_type, this->start_address, data);
        });
  } else {
    data = float_to_payload(write_value, this->sensor_value_type);

    ESP_LOGD(TAG,
             "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
             this->get_name().c_str(), this->start_address, this->register_count, value, write_value);

    // Create and send the write command
    if (this->register_count == 1 && !this->use_write_multiple_) {
      // since offset is in bytes and a register is 16 bits we get the start by adding offset/2
      write_cmd = ModbusCommandItem::create_write_single_command(this->parent_, this->start_address + this->offset / 2,
                                                                 data[0]);
    } else {
      write_cmd = ModbusCommandItem::create_write_multiple_command(
          this->parent_, this->start_address + this->offset / 2, this->register_count, data);
    }
    // publish new value
    write_cmd.on_data_func = [this, write_cmd, value](ModbusRegisterType register_type, uint16_t start_address,
                                                      const std::vector<uint8_t> &data) {
      // gets called when the write command is ack'd from the device
      this->parent_->on_write_register_response(write_cmd.register_type, start_address, data);
      this->publish_state(value);
    };
  }
  this->parent_->queue_command(write_cmd);
  this->publish_state(value);
}
void ModbusNumber::dump_config() { LOG_NUMBER(TAG, "Modbus Number", this); }

}  // namespace modbus_controller
}  // namespace esphome
