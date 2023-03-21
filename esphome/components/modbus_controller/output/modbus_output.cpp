#include "modbus_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.output";

/** Write a value to the device
 *
 */
void ModbusFloatOutput::write_state(float value) {
  std::vector<uint16_t> data;
  auto original_value = value;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, value, data);
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
  // lambda didn't set payload
  if (data.empty()) {
    data = float_to_payload(value, this->sensor_value_type);
  }

  ESP_LOGD(TAG, "Updating register: start address=0x%X register count=%d new value=%.02f (val=%.02f)",
           this->start_address, this->register_count, value, original_value);

  // Create and send the write command
  ModbusCommandItem write_cmd;
  if (this->register_count == 1 && !this->use_write_multiple_) {
    write_cmd = ModbusCommandItem::create_write_single_command(parent_, this->start_address + this->offset, data[0]);
  } else {
    write_cmd = ModbusCommandItem::create_write_multiple_command(parent_, this->start_address + this->offset,
                                                                 this->register_count, data);
  }
  parent_->queue_command(write_cmd);
}

void ModbusFloatOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Float Output:");
  LOG_FLOAT_OUTPUT(this);
  ESP_LOGCONFIG(TAG, "  Device start address: 0x%X", this->start_address);
  ESP_LOGCONFIG(TAG, "  Register count: %d", this->register_count);
  ESP_LOGCONFIG(TAG, "  Value type: %d", static_cast<int>(this->sensor_value_type));
}

// ModbusBinaryOutput
void ModbusBinaryOutput::write_state(bool state) {
  // This will be called every time the user requests a state change.
  ModbusCommandItem cmd;
  std::vector<uint8_t> data;

  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, state, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      state = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  }
  if (!data.empty()) {
    ESP_LOGV(TAG, "Modbus binary output write raw: %s", format_hex_pretty(data).c_str());
    cmd = ModbusCommandItem::create_custom_command(
        this->parent_, data,
        [this, cmd](ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
          this->parent_->on_write_register_response(cmd.register_type, this->start_address, data);
        });
  } else {
    ESP_LOGV(TAG, "Write new state: value is %s, type is %d address = %X, offset = %x", ONOFF(state),
             (int) this->register_type, this->start_address, this->offset);

    // offset for coil and discrete inputs is the coil/register number not bytes
    if (this->use_write_multiple_) {
      std::vector<bool> states{state};
      cmd = ModbusCommandItem::create_write_multiple_coils(parent_, this->start_address + this->offset, states);
    } else {
      cmd = ModbusCommandItem::create_write_single_coil(parent_, this->start_address + this->offset, state);
    }
  }
  this->parent_->queue_command(cmd);
}

void ModbusBinaryOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Binary Output:");
  LOG_BINARY_OUTPUT(this);
  ESP_LOGCONFIG(TAG, "  Device start address: 0x%X", this->start_address);
  ESP_LOGCONFIG(TAG, "  Register count: %d", this->register_count);
  ESP_LOGCONFIG(TAG, "  Value type: %d", static_cast<int>(this->sensor_value_type));
}

}  // namespace modbus_controller
}  // namespace esphome
