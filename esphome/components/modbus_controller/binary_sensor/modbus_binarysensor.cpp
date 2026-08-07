#include "modbus_binarysensor.h"
#include "esphome/core/log.h"

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller.binary_sensor";

void ModbusBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Modbus Controller Binary Sensor", this); }

void ModbusBinarySensor::parse_and_publish(std::span<const uint8_t> data) {
  bool value;
  // For coils/discrete inputs this is the bit index; for registers it is the byte offset.
  const size_t offset = this->offset;

  switch (this->register_type) {
    case modbus::EntityType::DISCRETE_INPUT:
    case modbus::EntityType::COIL:
      value = modbus::helpers::bit_from_packed(offset, data);
      break;
    default:
      value = modbus::helpers::get_data<uint16_t>(data.data(), offset) & this->bitmask;
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

}  // namespace esphome::modbus_controller
