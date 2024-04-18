#include "mpl115a2.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mpl115a2 {

static const char *const TAG = "mpl115a2";

void MPL115A2Component::setup() { ESP_LOGCONFIG(TAG, "Setting up MPL115A2..."); }

void MPL115A2Component::readCoefficients() {
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

  _mpl115a2_a0 = (float) a0coeff / 8;
  _mpl115a2_b1 = (float) b1coeff / 8192;
  _mpl115a2_b2 = (float) b2coeff / 16384;
  _mpl115a2_c12 = (float) c12coeff;
  _mpl115a2_c12 /= 4194304.0;
}

void MPL115A2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPL3115A2:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGE(TAG, "Communication with MPL3115A2 failed!");
        break;
      case WRONG_ID:
        ESP_LOGE(TAG, "MPL3115A2 has invalid id");
        break;
      default:
        ESP_LOGE(TAG, "Setting up MPL3115A2 registers failed!");
        break;
    }
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Pressure", this->pressure_);
}

void MPL115A2Component::update() {
  uint16_t pressure, temp;
  float pressureComp;

  uint8_t cmd[2] = {MPL115A2_REGISTER_STARTCONVERSION, 0};
  uint8_t buffer[4];

  this->write(cmd, 2);

  // Wait a bit for the conversion to complete (3ms max)
  delay(5);

  cmd[0] = MPL115A2_REGISTER_PRESSURE_MSB;
  this->write(cmd, 1);
  this->read(buffer, 4);

  pressure = (((uint16_t) buffer[0] << 8) | buffer[1]) >> 6;
  temp = (((uint16_t) buffer[2] << 8) | buffer[3]) >> 6;

  // See datasheet p.6 for evaluation sequence
  pressureComp = _mpl115a2_a0 + (_mpl115a2_b1 + _mpl115a2_c12 * temp) * pressure + _mpl115a2_b2 * temp;

  float pressure = ((65.0F / 1023.0F) * pressureComp) + 50.0F;
  if (this->pressure_ != nullptr)
    this->pressure_->publish_state(pressure);
  int16_t t = encode_uint16(buffer[3], buffer[4]);

  float temperature = ((float) temp - 498.0F) / -5.35F + 25.0F;
  if (this->temperature_ != nullptr)
    this->temperature_->publish_state(temperature);

  ESP_LOGD(TAG, "Got Temperature=%.1f°C Pressure=%.1f", temperature, pressure);

  this->status_clear_warning();
}

}  // namespace mpl115a2
}  // namespace esphome
