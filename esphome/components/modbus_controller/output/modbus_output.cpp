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
    switch (this->sensor_value_type) {
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
        ESP_LOGE("TAG", "Invalid data type for modbus output");
        return;
        break;
    }
  }

  ESP_LOGD(TAG, "Updating register: start address=0x%X register count=%d new value=%.02f (val=%.02f)",
           this->start_address, this->register_count, value, transformed_value);

  // Create and send the write command
  auto write_cmd =
      ModbusCommandItem::create_write_multiple_command(parent_, this->start_address, this->register_count, data);
  parent_->queue_command(write_cmd);
}

void ModbusOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Float Output:");
  //  LOG_PIN("  Pin: ", this->pin_);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace modbus_controller
}  // namespace esphome
