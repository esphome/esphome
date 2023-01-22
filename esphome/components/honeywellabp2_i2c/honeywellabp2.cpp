#include "honeywellabp2.h"
#include "esphome/core/log.h"

namespace esphome {
namespace honeywellabp2 {

static const char *const TAG = "honeywellabp2";

void HONEYWELLABP2Sensor::setup() {
  ESP_LOGD(TAG, "Setting up Honeywell ABP2 Sensor ");
  // Call request once
  read_sensor();
}

void HONEYWELLABP2Sensor::read_sensor() {
  if (write(i2c_cmd_, 3)) {
    mark_failed();
    return;
  }
  // Wait for request to complete
  delay(10);
  if (!read(raw_data_, 7)) {
    mark_failed();
    return;
  }
}

void HONEYWELLABP2Sensor::update() {
  ESP_LOGV(TAG, "Update Honeywell ABP2 Sensor");
  
  read_sensor();
  
  double press_counts = raw_data_[3] + raw_data_[2] * 256 + raw_data_[1] * 65536; // calculate digital pressure counts
  double temp_counts = raw_data_[6] + raw_data_[5] * 256 + raw_data_[4] * 65536; // calculate digital temperature counts
  
  if (pressure_sensor_ != nullptr) {
    float pressure = ((press_counts - min_count_) * (max_pressure_ - min_pressure_)) / (max_count_ - min_count_) + min_pressure_;
    pressure_sensor_->publish_state(pressure);
  }
  if (temperature_sensor_ != nullptr) {
    float temperature = (temp_counts * 200 / 16777215) - 50;
    temperature_sensor_->publish_state(temperature);
  }
}

void HONEYWELLABP2Sensor::dump_config() {
  //  LOG_SENSOR("", "HONEYWELLABP2", this);
  ESP_LOGCONFIG(TAG, "  Min Pressure Range: %0.1f", min_pressure_);
  ESP_LOGCONFIG(TAG, "  Max Pressure Range: %0.1f", max_pressure_);
  LOG_UPDATE_INTERVAL(this);
}

void HONEYWELLABP2Sensor::set_transfer_function(ABP2TRANFERFUNCTION transfer_function) {
  transfer_function_ = transfer_function;
  if (transfer_function_ == ABP2_TRANS_FUNC_B) {
    max_count_ = max_count_B_;
    min_count_ = min_count_B_;
  }
  else {
    max_count_ = max_count_A_;
    min_count_ = min_count_A_;
  }
}

}  // namespace honeywellabp
}  // namespace esphome
