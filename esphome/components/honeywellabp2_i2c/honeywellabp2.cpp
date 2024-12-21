#include "honeywellabp2.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace honeywellabp2_i2c {

static const uint8_t STATUS_BIT_POWER = 6;
static const uint8_t STATUS_BIT_BUSY = 5;
static const uint8_t STATUS_BIT_ERROR = 2;
static const uint8_t STATUS_MATH_SAT = 0;

static const char *const TAG = "honeywellabp2";

void HONEYWELLABP2Sensor::read_sensor_data() {
  if (this->read(raw_data_, 7) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with ABP2 failed!");
    this->status_set_warning("couldn't read sensor data");
    return;
  }
  float press_counts = encode_uint24(raw_data_[1], raw_data_[2], raw_data_[3]);  // calculate digital pressure counts
  float temp_counts = encode_uint24(raw_data_[4], raw_data_[5], raw_data_[6]);   // calculate digital temperature counts

  this->last_pressure_ = (((press_counts - this->min_count_) / (this->max_count_ - this->min_count_)) *
                          (this->max_pressure_ - this->min_pressure_)) +
                         this->min_pressure_;
  this->last_temperature_ = (temp_counts * 200 / 16777215) - 50;
  this->status_clear_warning();
}

void HONEYWELLABP2Sensor::start_measurement() {
  if (this->write(i2c_cmd_, 3) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with ABP2 failed!");
    this->status_set_warning("couldn't start measurement");
    return;
  }
  this->measurement_running_ = true;
}

bool HONEYWELLABP2Sensor::is_measurement_ready() {
  if (this->read(raw_data_, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with ABP2 failed!");
    this->status_set_warning("couldn't check measurement");
    return false;
  }
  if ((raw_data_[0] & (0x1 << STATUS_BIT_BUSY)) > 0) {
    return false;
  }
  this->measurement_running_ = false;
  return true;
}

void HONEYWELLABP2Sensor::measurement_timeout() {
  ESP_LOGE(TAG, "Timeout!");
  this->measurement_running_ = false;
  this->status_set_warning("measurement timed out");
}

float HONEYWELLABP2Sensor::get_pressure() { return this->last_pressure_; }

float HONEYWELLABP2Sensor::get_temperature() { return this->last_temperature_; }

void HONEYWELLABP2Sensor::loop() {
  if (this->measurement_running_) {
    if (this->is_measurement_ready()) {
      this->cancel_timeout("meas_timeout");

      this->read_sensor_data();
      if (pressure_sensor_ != nullptr) {
        this->pressure_sensor_->publish_state(this->get_pressure());
      }
      if (temperature_sensor_ != nullptr) {
        this->temperature_sensor_->publish_state(this->get_temperature());
      }
    }
  }
}

void HONEYWELLABP2Sensor::update() {
  ESP_LOGV(TAG, "Update Honeywell ABP2 Sensor");

  this->start_measurement();
  this->set_timeout("meas_timeout", 100, [this] { this->measurement_timeout(); });
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
    this->max_count_ = this->max_count_b_;
    this->min_count_ = this->min_count_b_;
  } else {
    this->max_count_ = this->max_count_a_;
    this->min_count_ = this->min_count_a_;
  }
}

}  // namespace honeywellabp2_i2c
}  // namespace esphome
