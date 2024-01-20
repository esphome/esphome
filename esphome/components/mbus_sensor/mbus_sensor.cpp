#include "mbus_sensor.h"

namespace esphome {
namespace mbus_sensor {

void MBusSensor::publish(const std::unique_ptr<mbus::MBusValue> &value) {
  // float result = payload_to_float(data, *this);

  // // Is there a lambda registered
  // // call it with the pre converted value and the raw data array
  // if (this->transform_func_.has_value()) {
  //   // the lambda can parse the response itself
  //   auto val = (*this->transform_func_)(this, result, data);
  //   if (val.has_value()) {
  //     ESP_LOGV(TAG, "Value overwritten by lambda");
  //     result = val.value();
  //   }
  // }
  // ESP_LOGD(TAG, "Sensor new state: %.02f", result);
  // // this->sensor_->raw_state = result;
  auto float_value = value->value;
  if (this->factor_ != 0) {
    float_value *= this->factor_;
  }

  this->publish_state(float_value);
}

}  // namespace mbus_sensor
}  // namespace esphome
