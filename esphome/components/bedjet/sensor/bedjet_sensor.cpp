#include "bedjet_sensor.h"

namespace esphome::bedjet {

std::string BedjetSensor::describe() { return "BedJet Sensor"; }

void BedjetSensor::on_bedjet_state(bool is_ready) {}

void BedjetSensor::on_status(const BedjetStatusPacket *data) {
  if (this->outlet_temperature_sensor_ != nullptr) {
    float converted_temp = bedjet_temp_to_c(data->actual_temp_step);
    if (converted_temp > 0) {
      this->outlet_temperature_sensor_->publish_state(converted_temp);
    }
  }

  if (this->ambient_temperature_sensor_ != nullptr) {
    float converted_temp = bedjet_temp_to_c(data->ambient_temp_step);
    if (converted_temp > 0) {
      this->ambient_temperature_sensor_->publish_state(converted_temp);
    }
  }
}

}  // namespace esphome::bedjet
