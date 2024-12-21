#include "qmc5883l.h"
#include "esphome/core/application.h"
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

  if (this->get_update_interval() < App.get_loop_interval()) {
    high_freq_.start();
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
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}
float QMC5883LComponent::get_setup_priority() const { return setup_priority::DATA; }
void QMC5883LComponent::update() {
  i2c::ErrorCode err;
  uint8_t status = false;
  // Status byte gets cleared when data is read, so we have to read this first.
  // If status and two axes are desired, it's possible to save one byte of traffic by enabling
  // ROL_PNT in setup and reading 7 bytes starting at the status register.
  // If status and all three axes are desired, using ROL_PNT saves you 3 bytes.
  // But simply not reading status saves you 4 bytes always and is much simpler.
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG) {
    err = this->read_register(QMC5883L_REGISTER_STATUS, &status, 1);
    if (err != i2c::ERROR_OK) {
      this->status_set_warning(str_sprintf("status read failed (%d)", err).c_str());
      return;
    }
  }

  uint16_t raw[3] = {0};
  // Z must always be requested, otherwise the data registers will remain locked against updates.
  // Skipping the Y axis if X and Z are needed actually requires an additional byte of comms.
  // Starting partway through the axes does save you traffic.
  uint8_t start, dest;
  if (this->heading_sensor_ != nullptr || this->x_sensor_ != nullptr) {
    start = QMC5883L_REGISTER_DATA_X_LSB;
    dest = 0;
  } else if (this->y_sensor_ != nullptr) {
    start = QMC5883L_REGISTER_DATA_Y_LSB;
    dest = 1;
  } else {
    start = QMC5883L_REGISTER_DATA_Z_LSB;
    dest = 2;
  }
  err = this->read_bytes_16_le_(start, &raw[dest], 3 - dest);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning(str_sprintf("mag read failed (%d)", err).c_str());
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
  const float x = int16_t(raw[0]) * mg_per_bit * 0.1f;
  const float y = int16_t(raw[1]) * mg_per_bit * 0.1f;
  const float z = int16_t(raw[2]) * mg_per_bit * 0.1f;

  float heading = atan2f(0.0f - x, y) * 180.0f / M_PI;

  float temp = NAN;
  if (this->temperature_sensor_ != nullptr) {
    uint16_t raw_temp;
    err = this->read_bytes_16_le_(QMC5883L_REGISTER_TEMPERATURE_LSB, &raw_temp);
    if (err != i2c::ERROR_OK) {
      this->status_set_warning(str_sprintf("temp read failed (%d)", err).c_str());
      return;
    }
    temp = int16_t(raw_temp) * 0.01f;
  }

  ESP_LOGD(TAG, "Got x=%0.02fµT y=%0.02fµT z=%0.02fµT heading=%0.01f° temperature=%0.01f°C status=%u", x, y, z, heading,
           temp, status);

  if (this->x_sensor_ != nullptr)
    this->x_sensor_->publish_state(x);
  if (this->y_sensor_ != nullptr)
    this->y_sensor_->publish_state(y);
  if (this->z_sensor_ != nullptr)
    this->z_sensor_->publish_state(z);
  if (this->heading_sensor_ != nullptr)
    this->heading_sensor_->publish_state(heading);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temp);
}

i2c::ErrorCode QMC5883LComponent::read_bytes_16_le_(uint8_t a_register, uint16_t *data, uint8_t len) {
  i2c::ErrorCode err = this->read_register(a_register, reinterpret_cast<uint8_t *>(data), len * 2);
  if (err != i2c::ERROR_OK)
    return err;
  for (size_t i = 0; i < len; i++)
    data[i] = convert_little_endian(data[i]);
  return err;
}

}  // namespace qmc5883l
}  // namespace esphome
