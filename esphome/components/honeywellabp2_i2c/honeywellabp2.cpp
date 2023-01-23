#include "honeywellabp2.h"
#include "esphome/core/log.h"

namespace esphome {
namespace honeywellabp2 {

static const char *const TAG = "honeywellabp2";

void HONEYWELLABP2Sensor::setup() {
  ESP_LOGD(TAG, "Setting up Honeywell ABP2 Sensor ");
  // Call request once
  this->read_sensor();
}

void HONEYWELLABP2Sensor::read_sensor() {
  if (this->write(i2c_cmd_, 3)) {
    this->mark_failed();
    return;
  }
  // Wait for request to complete
  delay(10);
  if (!this->read(raw_data_, 7)) {
    this->mark_failed();
    return;
  }
  float press_counts = raw_data_[3] + raw_data_[2] * 256 + raw_data_[1] * 65536; // calculate digital pressure counts
  float temp_counts = raw_data_[6] + raw_data_[5] * 256 + raw_data_[4] * 65536; // calculate digital temperature counts
  
  this->last_pressure_ = ((press_counts - this->min_count_) * (double)(this->max_pressure_ - this->min_pressure_)) / (this->max_count_ - this->min_count_) + this->min_pressure_;
  this->last_temperature_ = (temp_counts * 200 / 16777215) - 50;
}

float HONEYWELLABP2Sensor::get_pressure() {
  return this->last_pressure_;
}

float HONEYWELLABP2Sensor::get_temperature() {
  return this->last_temperature_;
}

void HONEYWELLABP2Sensor::update() {
  ESP_LOGV(TAG, "Update Honeywell ABP2 Sensor");
  
  this->read_sensor();
  
  if (pressure_sensor_ != nullptr) {
    this->pressure_sensor_->publish_state(this->get_pressure());
  }
  if (temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(this->get_temperature());
  }
}

void HONEYWELLABP2Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "  Min Pressure Range: %0.1f", this->min_pressure_);
  ESP_LOGCONFIG(TAG, "  Max Pressure Range: %0.1f", this->max_pressure_);
  if (this->transfer_function_ == ABP2_TRANS_FUNC_A) {
    ESP_LOGCONFIG(TAG, "  Transfer function A");
  } else {
    ESP_LOGCONFIG(TAG, "  Transfer function B");
  }
  LOG_UPDATE_INTERVAL(this);
}

void HONEYWELLABP2Sensor::set_transfer_function(ABP2TRANFERFUNCTION transfer_function) {
  this->transfer_function_ = transfer_function;
  if (this->transfer_function_ == ABP2_TRANS_FUNC_B) {
    this->max_count_ = this->max_count_B_;
    this->min_count_ = this->min_count_B_;
  } else {
    this->max_count_ = this->max_count_A_;
    this->min_count_ = this->min_count_A_;
  }
}

}  // namespace honeywellabp2
}  // namespace esphome
