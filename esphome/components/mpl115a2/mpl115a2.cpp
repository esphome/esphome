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

  a0coeff = encode_uint16(buffer[0], buffer[1]);
  b1coeff = encode_uint16(buffer[2], buffer[3]);
  b2coeff = encode_uint16(buffer[4], buffer[5]);
  c12coeff = encode_uint16(buffer[6], buffer[7]) >> 2;

  this->mpl115a2_a0_ = a0coeff / 8.0f;
  this->mpl115a2_b1_ = b1coeff / 8192.0f;
  this->mpl115a2_b2_ = b2coeff / 16384.0f;
  this->mpl115a2_c12_ = c12coeff / 4194304.0f;
}

void MPL115A2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPL115A2:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Pressure", this->pressure_);
}

void MPL115A2Component::update() {
  uint8_t cmd[2] = {MPL115A2_REGISTER_STARTCONVERSION, 0};
  this->write(cmd, 2);

  // Wait a bit for the conversion to complete (3ms max)
  this->set_timeout(5, [this, cmd]() {
    uint8_t buffer[4];
    cmd[0] = MPL115A2_REGISTER_PRESSURE_MSB;
    this->write(cmd, 1);
    this->read(buffer, 4);

    uint16_t pressure_in = encode_uint16(buffer[0], buffer[1]) >> 6;
    uint16_t temp_in = encode_uint16(buffer[2], buffer[3]) >> 6;

    // See datasheet p.6 for evaluation sequence
    float pressure_comp =
        mpl115a2_a0_ + (mpl115a2_b1_ + mpl115a2_c12_ * temp_in) * pressure_in + mpl115a2_b2_ * temp_in;

    float pressure_out = ((65.0f / 1023.0f) * pressure_comp) + 50.0f;
    if (this->pressure_ != nullptr)
      this->pressure_->publish_state(pressure_out);

    float temperature_out = ((float) temp_in - 498.0f) / -5.35f + 25.0f;
    if (this->temperature_ != nullptr)
      this->temperature_->publish_state(temperature_out);

    ESP_LOGD(TAG, "Got Temperature=%.1f°C Pressure=%.1f", temperature_out, pressure_out);
  });
}

}  // namespace mpl115a2
}  // namespace esphome
