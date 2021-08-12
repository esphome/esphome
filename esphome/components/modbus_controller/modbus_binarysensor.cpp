#include "esphome/core/application.h"
#include "modbus_binarysensor.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_binarysensor";

// ModbusBinarySensor
void ModbusBinarySensor::add_to_controller(ModbusController *master, ModbusFunctionCode register_type,
                                           uint16_t start_address, uint8_t offset, uint32_t bitmask,
                                           uint8_t skip_updates) {
  this->register_type = register_type;
  this->start_address = start_address;
  this->offset = offset;
  this->bitmask = bitmask;
  this->sensor_value_type = SensorValueType::BIT;
  if (register_type == ModbusFunctionCode::READ_COILS || register_type == ModbusFunctionCode::READ_DISCRETE_INPUTS)
    this->register_count = offset + 1;
  else
    this->register_count = 1;

  this->skip_updates = skip_updates;
  this->parent_ = master;
  master->add_sensor_item(this);
}

void ModbusBinarySensor::log() { LOG_BINARY_SENSOR(TAG, get_name().c_str(), this); }

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
// ModbusBinarySensor End

}  // namespace modbus_controller
}  // namespace esphome

/*


*/
