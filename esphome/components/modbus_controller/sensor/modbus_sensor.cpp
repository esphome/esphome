
#include "modbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.sensor";

void ModbusSensor::dump_config() { LOG_SENSOR(TAG, "Modbus Controller Sensor", this); }

void ModbusSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  union {
    float float_value;
    uint32_t raw;
  } raw_to_float;

  float result = payload_to_float(data, *this);

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->transform_func_)(result, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      result = val.value();
    } else {
      ESP_LOGV(TAG, "publishing handled by lambda - parse and publish");
      return;
    }
  }

  // No need to publish if the value didn't change since the last publish
  // can reduce mqtt traffic considerably if many sensors are used
  ESP_LOGD(TAG, " SENSOR : new: %.02f", result);
  // this->sensor_->raw_state = result;
  this->publish_state(result);
}

}  // namespace modbus_controller
}  // namespace esphome
