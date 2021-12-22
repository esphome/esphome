#include <vector>
#include "modbus_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.output";

void ModbusOutput::setup() {}

/** Write a value to the device
 *
 */
void ModbusOutput::write_state(float value) {
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

void ModbusOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Float Output:");
  LOG_FLOAT_OUTPUT(this);
  ESP_LOGCONFIG(TAG, "Modbus device start address=0x%X register count=%d value type=%hhu", this->start_address,
                this->register_count, this->sensor_value_type);
}

}  // namespace modbus_controller
}  // namespace esphome
