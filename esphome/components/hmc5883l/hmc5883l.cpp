#include "hmc5883l.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hmc5883l {

static const char *const TAG = "hmc5883l";
static const uint8_t HMC5883L_ADDRESS = 0x1E;
static const uint8_t HMC5883L_REGISTER_CONFIG_A = 0x00;
static const uint8_t HMC5883L_REGISTER_CONFIG_B = 0x01;
static const uint8_t HMC5883L_REGISTER_MODE = 0x02;
static const uint8_t HMC5883L_REGISTER_DATA_X_MSB = 0x03;
static const uint8_t HMC5883L_REGISTER_DATA_X_LSB = 0x04;
static const uint8_t HMC5883L_REGISTER_DATA_Z_MSB = 0x05;
static const uint8_t HMC5883L_REGISTER_DATA_Z_LSB = 0x06;
static const uint8_t HMC5883L_REGISTER_DATA_Y_MSB = 0x07;
static const uint8_t HMC5883L_REGISTER_DATA_Y_LSB = 0x08;
static const uint8_t HMC5883L_REGISTER_STATUS = 0x09;
static const uint8_t HMC5883L_REGISTER_IDENTIFICATION_A = 0x0A;
static const uint8_t HMC5883L_REGISTER_IDENTIFICATION_B = 0x0B;
static const uint8_t HMC5883L_REGISTER_IDENTIFICATION_C = 0x0C;

void HMC5883LComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HMC5583L...");
  uint8_t id[3];
  if (!this->read_byte(HMC5883L_REGISTER_IDENTIFICATION_A, &id[0]) ||
      !this->read_byte(HMC5883L_REGISTER_IDENTIFICATION_B, &id[1]) ||
      !this->read_byte(HMC5883L_REGISTER_IDENTIFICATION_C, &id[2])) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (id[0] != 0x48 || id[1] != 0x34 || id[2] != 0x33) {
    this->error_code_ = ID_REGISTERS;
    this->mark_failed();
    return;
  }

  uint8_t config_a = 0;
  config_a |= this->oversampling_ << 5;
  config_a |= this->datarate_ << 2;
  config_a |= 0b0 << 0;  // Measurement Mode: Normal(high impedance on load)
  if (!this->write_byte(HMC5883L_REGISTER_CONFIG_A, config_a)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint8_t config_b = 0;
  config_b |= this->range_ << 5;
  if (!this->write_byte(HMC5883L_REGISTER_CONFIG_B, config_b)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint8_t mode = 0;
  // Continuous Measurement Mode
  mode |= 0b00;
  if (!this->write_byte(HMC5883L_REGISTER_MODE, mode)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
}
void HMC5883LComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HMC5883L:");
  LOG_I2C_DEVICE(this);
  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with HMC5883L failed!");
  } else if (this->error_code_ == ID_REGISTERS) {
    ESP_LOGE(TAG, "The ID registers don't match - Is this really an HMC5883L?");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
  LOG_SENSOR("  ", "Heading", this->heading_sensor_);
}
float HMC5883LComponent::get_setup_priority() const { return setup_priority::DATA; }
void HMC5883LComponent::update() {
  uint16_t raw_x, raw_y, raw_z;
  if (!this->read_byte_16(HMC5883L_REGISTER_DATA_X_MSB, &raw_x) ||
      !this->read_byte_16(HMC5883L_REGISTER_DATA_Y_MSB, &raw_y) ||
      !this->read_byte_16(HMC5883L_REGISTER_DATA_Z_MSB, &raw_z)) {
    this->status_set_warning();
    return;
  }

  float mg_per_bit;
  switch (this->range_) {
    case HMC5883L_RANGE_88_UT:
      mg_per_bit = 0.073f;
      break;
    case HMC5883L_RANGE_130_UT:
      mg_per_bit = 0.92f;
      break;
    case HMC5883L_RANGE_190_UT:
      mg_per_bit = 1.22f;
      break;
    case HMC5883L_RANGE_250_UT:
      mg_per_bit = 1.52f;
      break;
    case HMC5883L_RANGE_400_UT:
      mg_per_bit = 2.27f;
      break;
    case HMC5883L_RANGE_470_UT:
      mg_per_bit = 2.56f;
      break;
    case HMC5883L_RANGE_560_UT:
      mg_per_bit = 3.03f;
      break;
    case HMC5883L_RANGE_810_UT:
      mg_per_bit = 4.35f;
      break;
    default:
      mg_per_bit = NAN;
  }

  // in µT
  const float x = int16_t(raw_x) * mg_per_bit * 0.1f;
  const float y = int16_t(raw_y) * mg_per_bit * 0.1f;
  const float z = int16_t(raw_z) * mg_per_bit * 0.1f;

  float heading = atan2f(0.0f - x, y) * 180.0f / M_PI;
  ESP_LOGD(TAG, "Got x=%0.02fµT y=%0.02fµT z=%0.02fµT heading=%0.01f°", x, y, z, heading);

  if (this->x_sensor_ != nullptr)
    this->x_sensor_->publish_state(x);
  if (this->y_sensor_ != nullptr)
    this->y_sensor_->publish_state(y);
  if (this->z_sensor_ != nullptr)
    this->z_sensor_->publish_state(z);
  if (this->heading_sensor_ != nullptr)
    this->heading_sensor_->publish_state(heading);
}

}  // namespace hmc5883l
}  // namespace esphome
