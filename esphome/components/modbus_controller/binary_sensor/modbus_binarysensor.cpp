#include "modbus_binarysensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.binary_sensor";

void ModbusBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Modbus Controller Binary Sensor", this); }

void ModbusBinarySensor::parse_and_publish(const std::vector<uint8_t> &data) {
  bool value;

  switch (this->register_type) {
    case ModbusRegisterType::DISCRETE_INPUT:
    case ModbusRegisterType::COIL:
      // offset for coil is the actual number of the coil not the byte offset
      value = coil_from_vector(this->offset, data);
      break;
    default:
      value = get_data<uint16_t>(data, this->offset) & this->bitmask;
      break;
  }
  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      value = val.value();
    }
  }
  this->publish_state(value);
}

}  // namespace modbus_controller
}  // namespace esphome
