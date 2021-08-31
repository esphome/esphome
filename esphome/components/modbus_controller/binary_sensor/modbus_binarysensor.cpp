#include "modbus_binarysensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.binary_sensor";

void ModbusBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Modbus Controller Binary Sensor", this); }

float ModbusBinarySensor::parse_and_publish(const std::vector<uint8_t> &data) {
  int64_t value = 0;
  float result = NAN;
  switch (this->register_type) {
    case ModbusFunctionCode::READ_DISCRETE_INPUTS:
      value = coil_from_vector(this->offset, data);
      break;
    case ModbusFunctionCode::READ_COILS:
      // offset for coil is the actual number of the coil not the byte offset
      value = coil_from_vector(this->offset, data);
      break;
    default:
      value = get_data<uint16_t>(data, this->offset) & this->bitmask;
      break;
  }

  result = static_cast<float>(value);
  this->publish_state(value != 0.0);
  return result;
}

}  // namespace modbus_controller
}  // namespace esphome
