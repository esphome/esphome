#include <vector>
#include "modbus_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_device {

static const char *const TAG = "modbus.number";

void ModbusNumber::add_values_to_payload(std::vector<uint16_t> &payload, ssize_t offset) {
  std::vector<uint16_t> data;
  float result = this->state;

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->read_transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->read_transform_func_)(this, result, data);
    ESP_LOGV(TAG, "Returned from transform func %s", format_hex_pretty(data).c_str());
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      result = val.value();
    }
  }
  if (data.empty()) {
    data = float_to_payload(result, this->sensor_value_type);
  }
  if(offset + data.size() > payload.size()) {
    payload.resize(offset + data.size());
  }
  ESP_LOGV(TAG, "Adding to payload[0x%02x] length: %u %s[%f]", offset, data.size(), format_hex_pretty(data).c_str(), result);
  for(int i = 0; i < data.size(); ++i) {
    ESP_LOGV(TAG, "Data %02x %04x", offset + i, data[i]);
    payload[offset + i] = data[i];
  }
}

void ModbusNumber::control(float value) {
  // Is there are lambda configured?
  if (this->transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->transform_func_)(this, value);
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
  this->publish_state(value);
}
void ModbusNumber::dump_config() { LOG_NUMBER(TAG, "Modbus Number", this); }

}  // namespace modbus_controller
}  // namespace esphome
