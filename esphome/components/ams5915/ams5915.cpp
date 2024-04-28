#include "ams5915.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ams5915 {
static const char *const TAG = "ams5915";

void Ams5915::set_transducer_type(Transducer model) { type_ = model; }

/* starts the I2C communication and sets the pressure and temperature ranges using getTransducer */
int Ams5915::begin_() {
  // setting the min and max pressure based on the chip
  this->get_transducer_();
  // checking to see if we can talk with the sensor
  for (size_t i = 0; i < max_attempts_; i++) {
    status_ = read_bytes_(&pressure_counts_, &temperature_counts_);
    if (status_ > 0) {
      break;
    }
    delay(10);
  }
  return status_;
}

/* reads data from the sensor */
int Ams5915::read_sensor_() {
  // get pressure and temperature counts off transducer
  this->status_ = read_bytes_(&this->pressure_counts_, &this->temperature_counts_);
  // convert counts to pressure, PA
  this->data_.pressure_pa_ =
      (((float) (this->pressure_counts_ - this->dig_out_p_min_)) /
           (((float) (this->dig_out_p_max_ - this->dig_out_p_min_)) / ((float) (this->p_max_ - this->p_min_))) +
       (float) this->p_min_);
  // convert counts to temperature, C
  this->data_.temperature_c_ = (float) ((this->temperature_counts_ * 200)) / 2048.0f - 50.0f;
  return this->status_;
}

/* returns the pressure value, PA */
float Ams5915::get_pressure_pa_() { return this->data_.pressure_pa_; }

/* returns the temperature value, C */
float Ams5915::get_temperature_c_() { return this->data_.temperature_c_; }

/* sets the pressure range based on the chip */
void Ams5915::get_transducer_() {
  // setting the min and max pressures based on which transducer it is
  switch (this->type_) {
    case AMS5915_0005_D:
      this->p_min_ = this->ams5915_0005_d_p_min_;
      this->p_max_ = this->ams5915_0005_d_p_max_;
      break;
    case AMS5915_0010_D:
      this->p_min_ = this->ams5915_0010_d_p_min_;
      this->p_max_ = this->ams5915_0010_d_p_max_;
      break;
    case AMS5915_0005_D_B:
      this->p_min_ = this->ams5915_0005_d_b_p_min_;
      this->p_max_ = this->ams5915_0005_d_b_p_max_;
      break;
    case AMS5915_0010_D_B:
      this->p_min_ = this->ams5915_0010_d_b_p_min_;
      this->p_max_ = this->ams5915_0010_d_b_p_max_;
      break;
    case AMS5915_0020_D:
      this->p_min_ = this->ams5915_0020_d_p_min_;
      this->p_max_ = this->ams5915_0020_d_p_max_;
      break;
    case AMS5915_0050_D:
      this->p_min_ = this->ams5915_0050_d_p_min_;
      this->p_max_ = this->ams5915_0050_d_p_max_;
      break;
    case AMS5915_0100_D:
      this->p_min_ = this->ams5915_0100_d_p_min_;
      this->p_max_ = this->ams5915_0100_d_p_max_;
      break;
    case AMS5915_0020_D_B:
      this->p_min_ = this->ams5915_0020_d_b_p_min_;
      this->p_max_ = this->ams5915_0020_d_b_p_max_;
      break;
    case AMS5915_0050_D_B:
      this->p_min_ = this->ams5915_0050_d_b_p_min_;
      this->p_max_ = this->ams5915_0050_d_b_p_max_;
      break;
    case AMS5915_0100_D_B:
      this->p_min_ = this->ams5915_0100_d_b_p_min_;
      this->p_max_ = this->ams5915_0100_d_b_p_max_;
      break;
    case AMS5915_0200_D:
      this->p_min_ = this->ams5915_0200_d_p_min_;
      this->p_max_ = this->ams5915_0200_d_p_max_;
      break;
    case AMS5915_0350_D:
      this->p_min_ = this->ams5915_0350_d_p_min_;
      this->p_max_ = this->ams5915_0350_d_p_max_;
      break;
    case AMS5915_1000_D:
      this->p_min_ = this->ams5915_1000_d_p_min_;
      this->p_max_ = this->ams5915_1000_d_p_max_;
      break;
    case AMS5915_2000_D:
      this->p_min_ = this->ams5915_2000_d_p_min_;
      this->p_max_ = this->ams5915_2000_d_p_max_;
      break;
    case AMS5915_4000_D:
      this->p_min_ = this->ams5915_4000_d_p_min_;
      this->p_max_ = this->ams5915_4000_d_p_max_;
      break;
    case AMS5915_7000_D:
      this->p_min_ = this->ams5915_7000_d_p_min_;
      this->p_max_ = this->ams5915_7000_d_p_max_;
      break;
    case AMS5915_10000_D:
      this->p_min_ = this->ams5915_10000_d_p_min_;
      this->p_max_ = this->ams5915_10000_d_p_max_;
      break;
    case AMS5915_0200_D_B:
      this->p_min_ = this->ams5915_0200_d_b_p_min_;
      this->p_max_ = this->ams5915_0200_d_b_p_max_;
      break;
    case AMS5915_0350_D_B:
      this->p_min_ = this->ams5915_0350_d_b_p_min_;
      this->p_max_ = this->ams5915_0350_d_b_p_max_;
      break;
    case AMS5915_1000_D_B:
      this->p_min_ = this->ams5915_1000_d_b_p_min_;
      this->p_max_ = this->ams5915_1000_d_b_p_max_;
      break;
    case AMS5915_1000_A:
      this->p_min_ = this->ams5915_1000_a_p_min_;
      this->p_max_ = this->ams5915_1000_a_p_max_;
      break;
    case AMS5915_1200_B:
      this->p_min_ = this->ams5915_1200_b_p_min_;
      this->p_max_ = this->ams5915_1200_b_p_max_;
      break;
  }
}

/* reads pressure and temperature and returns values in counts */
int Ams5915::read_bytes_(uint16_t *pressure_counts, uint16_t *temperature_counts) {
  i2c::ErrorCode err = this->read(this->buffer_, sizeof(this->buffer_));
  if (err != i2c::ERROR_OK) {
    this->status_ = -1;
  } else {
    *pressure_counts = (((uint16_t) (this->buffer_[0] & 0x3F)) << 8) + (((uint16_t) this->buffer_[1]));
    *temperature_counts = (((uint16_t) (this->buffer_[2])) << 3) + (((uint16_t) this->buffer_[3] & 0xE0) >> 5);
    this->status_ = 1;
  }
  return this->status_;
}

void Ams5915::setup() {
  if (this->begin_() < 0) {
    ESP_LOGE(TAG, "Failed to read pressure from Ams5915");
    this->mark_failed();
  }
}

void Ams5915::update() {
  this->read_sensor_();
  float temperature = this->get_temperature_c_();
  float pressure = this->get_pressure_pa_();

  ESP_LOGD(TAG, "Got pressure=%.3fmBar %.3fpa temperature=%.1f°C", pressure, pressure * this->mbar_to_pa_, temperature);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure * this->mbar_to_pa_);
}

void Ams5915::dump_config() {
  ESP_LOGCONFIG(TAG, "Ams5915:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
}

}  // namespace ams5915
}  // namespace esphome
