#include "bedjet_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bedjet {

std::string BedjetSensor::describe() { return "BedJet Sensor"; }

void BedjetSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "BedJet Sensor:");
  LOG_SENSOR("  ", "Outlet Temperature", this->outlet_temperature_sensor_);
  LOG_SENSOR("  ", "Ambient Temperature", this->ambient_temperature_sensor_);
}

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

}  // namespace bedjet
}  // namespace esphome
