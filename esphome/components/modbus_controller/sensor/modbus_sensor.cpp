
#include "modbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.sensor";

void ModbusSensor::dump_config() { LOG_SENSOR("", "Modbus Controller Sensor", this); }

void ModbusSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  union {
    float float_value;
    uint32_t raw;
  } raw_to_float;

  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits
  float result = payload_to_float(data, *this);

  // No need to publish if the value didn't change since the last publish
  // can reduce mqtt traffic considerably if many sensors are used
  ESP_LOGD(TAG, " SENSOR : new: %lld", value);
  // this->sensor_->raw_state = result;
  this->publish_state(result);
}

}  // namespace modbus_controller
}  // namespace esphome
