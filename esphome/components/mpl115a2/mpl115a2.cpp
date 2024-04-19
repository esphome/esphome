#include "mpl115a2.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mpl115a2 {

static const char *const TAG = "mpl115a2";

void MPL115A2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MPL115A2...");

  this->read_coefficients_();
}

void MPL115A2Component::read_coefficients_() {
  int16_t a0coeff;
  int16_t b1coeff;
  int16_t b2coeff;
  int16_t c12coeff;

  uint8_t cmd;
  uint8_t buffer[8];

  cmd = MPL115A2_REGISTER_A0_COEFF_MSB;
  this->write(&cmd, 1);
  this->read(buffer, 8);

  a0coeff = (((uint16_t) buffer[0] << 8) | buffer[1]);
  b1coeff = (((uint16_t) buffer[2] << 8) | buffer[3]);
  b2coeff = (((uint16_t) buffer[4] << 8) | buffer[5]);
  c12coeff = (((uint16_t) buffer[6] << 8) | buffer[7]) >> 2;

  mpl115a2_a0_ = (float) a0coeff / 8;
  mpl115a2_b1_ = (float) b1coeff / 8192;
  mpl115a2_b2_ = (float) b2coeff / 16384;
  mpl115a2_c12_ = (float) c12coeff;
  mpl115a2_c12_ /= 4194304.0;
}

void MPL115A2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPL115A2:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Pressure", this->pressure_);
}

void MPL115A2Component::update() {
  uint16_t pressure_in, temp_in;
  float pressure_comp;

  uint8_t cmd[2] = {MPL115A2_REGISTER_STARTCONVERSION, 0};
  uint8_t buffer[4];

  this->write(cmd, 2);

  // Wait a bit for the conversion to complete (3ms max)
  delay(5);

  cmd[0] = MPL115A2_REGISTER_PRESSURE_MSB;
  this->write(cmd, 1);
  this->read(buffer, 4);

  pressure_in = (((uint16_t) buffer[0] << 8) | buffer[1]) >> 6;
  temp_in = (((uint16_t) buffer[2] << 8) | buffer[3]) >> 6;

  // See datasheet p.6 for evaluation sequence
  pressure_comp = mpl115a2_a0_ + (mpl115a2_b1_ + mpl115a2_c12_ * temp_in) * pressure_in + mpl115a2_b2_ * temp_in;

  float pressure_out = ((65.0F / 1023.0F) * pressure_comp) + 50.0F;
  if (this->pressure_ != nullptr)
    this->pressure_->publish_state(pressure_out);

  float temperature_out = ((float) temp_in - 498.0F) / -5.35F + 25.0F;
  if (this->temperature_ != nullptr)
    this->temperature_->publish_state(temperature_out);

  ESP_LOGD(TAG, "Got Temperature=%.1f°C Pressure=%.1f", temperature_out, pressure_out);

  this->status_clear_warning();
}

}  // namespace mpl115a2
}  // namespace esphome
