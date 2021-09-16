#include "qmc5883l.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>

namespace esphome {
namespace qmc5883l {

static const char *const TAG = "qmc5883l";
static const uint8_t QMC5883L_ADDRESS = 0x0D;

static const uint8_t QMC5883L_REGISTER_DATA_X_LSB = 0x00;
static const uint8_t QMC5883L_REGISTER_DATA_X_MSB = 0x01;
static const uint8_t QMC5883L_REGISTER_DATA_Y_LSB = 0x02;
static const uint8_t QMC5883L_REGISTER_DATA_Y_MSB = 0x03;
static const uint8_t QMC5883L_REGISTER_DATA_Z_LSB = 0x04;
static const uint8_t QMC5883L_REGISTER_DATA_Z_MSB = 0x05;
static const uint8_t QMC5883L_REGISTER_STATUS = 0x06;
static const uint8_t QMC5883L_REGISTER_TEMPERATURE_LSB = 0x07;
static const uint8_t QMC5883L_REGISTER_TEMPERATURE_MSB = 0x08;
static const uint8_t QMC5883L_REGISTER_CONTROL_1 = 0x09;
static const uint8_t QMC5883L_REGISTER_CONTROL_2 = 0x0A;
static const uint8_t QMC5883L_REGISTER_PERIOD = 0x0B;

void QMC5883LComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up QMC5883L...");
  // Soft Reset
  if (!this->write_byte(QMC5883L_REGISTER_CONTROL_2, 1 << 7)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  delay(10);

  uint8_t control_1 = 0;
  control_1 |= 0b01 << 0;  // MODE (Mode) -> 0b00=standby, 0b01=continuous
  control_1 |= this->datarate_ << 2;
  control_1 |= this->range_ << 4;
  control_1 |= this->oversampling_ << 6;
  if (!this->write_byte(QMC5883L_REGISTER_CONTROL_1, control_1)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint8_t control_2 = 0;
  control_2 |= 0b0 << 7;  // SOFT_RST (Soft Reset) -> 0b00=disabled, 0b01=enabled
  control_2 |= 0b0 << 6;  // ROL_PNT (Pointer Roll Over) -> 0b00=disabled, 0b01=enabled
  control_2 |= 0b0 << 0;  // INT_ENB (Interrupt) -> 0b00=disabled, 0b01=enabled
  if (!this->write_byte(QMC5883L_REGISTER_CONTROL_2, control_2)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint8_t period = 0x01;  // recommended value
  if (!this->write_byte(QMC5883L_REGISTER_PERIOD, period)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
}
void QMC5883LComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "QMC5883L:");
  LOG_I2C_DEVICE(this);
  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with QMC5883L failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
  LOG_SENSOR("  ", "Heading", this->heading_sensor_);
}
float QMC5883LComponent::get_setup_priority() const { return setup_priority::DATA; }
void QMC5883LComponent::update() {
  uint8_t status = false;
  this->read_byte(QMC5883L_REGISTER_STATUS, &status);

  uint16_t raw_x, raw_y, raw_z;
  if (!this->read_byte_16_(QMC5883L_REGISTER_DATA_X_LSB, &raw_x) ||
      !this->read_byte_16_(QMC5883L_REGISTER_DATA_Y_LSB, &raw_y) ||
      !this->read_byte_16_(QMC5883L_REGISTER_DATA_Z_LSB, &raw_z)) {
    this->status_set_warning();
    return;
  }

  float mg_per_bit;
  switch (this->range_) {
    case QMC5883L_RANGE_200_UT:
      mg_per_bit = 0.0833f;
      break;
    case QMC5883L_RANGE_800_UT:
      mg_per_bit = 0.333f;
      break;
    default:
      mg_per_bit = NAN;
  }

  // in µT
  const float x = int16_t(raw_x) * mg_per_bit * 0.1f;
  const float y = int16_t(raw_y) * mg_per_bit * 0.1f;
  const float z = int16_t(raw_z) * mg_per_bit * 0.1f;

  float heading = atan2f(0.0f - x, y) * 180.0f / M_PI;
  ESP_LOGD(TAG, "Got x=%0.02fµT y=%0.02fµT z=%0.02fµT heading=%0.01f° status=%u", x, y, z, heading, status);

  if (this->x_sensor_ != nullptr)
    this->x_sensor_->publish_state(x);
  if (this->y_sensor_ != nullptr)
    this->y_sensor_->publish_state(y);
  if (this->z_sensor_ != nullptr)
    this->z_sensor_->publish_state(z);
  if (this->heading_sensor_ != nullptr)
    this->heading_sensor_->publish_state(heading);
}

bool QMC5883LComponent::read_byte_16_(uint8_t a_register, uint16_t *data) {
  if (!this->read_byte_16(a_register, data))
    return false;
  *data = (*data & 0x00FF) << 8 | (*data & 0xFF00) >> 8;  // Flip Byte order, LSB first;
  return true;
}

}  // namespace qmc5883l
}  // namespace esphome
