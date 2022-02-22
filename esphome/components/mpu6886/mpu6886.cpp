#include "mpu6886.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mpu6886 {

static const char *const TAG = "mpu6886";

const uint8_t MPU6886_REGISTER_WHO_AM_I = 0x75;
const uint8_t MPU6886_REGISTER_POWER_MANAGEMENT_1 = 0x6B;
const uint8_t MPU6886_REGISTER_GYRO_CONFIG = 0x1B;
const uint8_t MPU6886_REGISTER_ACCEL_CONFIG = 0x1C;
const uint8_t MPU6886_REGISTER_ACCEL_XOUT_H = 0x3B;
const uint8_t MPU6886_CLOCK_SOURCE_X_GYRO = 0b001;
const uint8_t MPU6886_SCALE_2000_DPS = 0b11;
const uint8_t MPU6886_WHO_AM_I_IDENTIFIER = 0x19;
const float MPU6886_SCALE_DPS_PER_DIGIT_2000 = 0.060975f;
const uint8_t MPU6886_RANGE_2G = 0b00;
const float MPU6886_RANGE_PER_DIGIT_2G = 0.000061f;
const uint8_t MPU6886_BIT_SLEEP_ENABLED = 6;
const uint8_t MPU6886_BIT_TEMPERATURE_DISABLED = 3;
const float GRAVITY_EARTH = 9.80665f;
// See https://github.com/m5stack/M5-Schematic/blob/master/datasheet/MPU-6886-000193%2Bv1.1_GHIC.PDF.pdf
// p. 43
const float TEMPERATURE_SENSITIVITY = 326.8;
const float TEMPERATURE_OFFSET = 25.0;

void MPU6886Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MPU6886...");
  uint8_t who_am_i;
  if (!this->read_byte(MPU6886_REGISTER_WHO_AM_I, &who_am_i) || who_am_i != MPU6886_WHO_AM_I_IDENTIFIER) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up Power Management...");
  // Setup power management
  uint8_t power_management;
  if (!this->read_byte(MPU6886_REGISTER_POWER_MANAGEMENT_1, &power_management)) {
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "  Input power_management: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(power_management));
  // Set clock source - X-Gyro
  power_management &= 0b11111000;
  power_management |= MPU6886_CLOCK_SOURCE_X_GYRO;
  // Disable sleep
  power_management &= ~(1 << MPU6886_BIT_SLEEP_ENABLED);
  // Enable temperature
  power_management &= ~(1 << MPU6886_BIT_TEMPERATURE_DISABLED);
  ESP_LOGV(TAG, "  Output power_management: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(power_management));
  if (!this->write_byte(MPU6886_REGISTER_POWER_MANAGEMENT_1, power_management)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up Gyroscope Config...");
  // Set scale - 2000DPS
  uint8_t gyro_config;
  if (!this->read_byte(MPU6886_REGISTER_GYRO_CONFIG, &gyro_config)) {
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "  Input gyroscope_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_config));
  gyro_config &= 0b11100111;
  gyro_config |= MPU6886_SCALE_2000_DPS << 3;
  ESP_LOGV(TAG, "  Output gyro_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_config));
  if (!this->write_byte(MPU6886_REGISTER_GYRO_CONFIG, gyro_config)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up Accelerometer Config...");
  // Set range - 2G
  uint8_t accel_config;
  if (!this->read_byte(MPU6886_REGISTER_ACCEL_CONFIG, &accel_config)) {
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "    Input accelerometer_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_config));
  accel_config &= 0b11100111;
  accel_config |= (MPU6886_RANGE_2G << 3);
  ESP_LOGV(TAG, "    Output accel_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_config));
  if (!this->write_byte(MPU6886_REGISTER_GYRO_CONFIG, gyro_config)) {
    this->mark_failed();
    return;
  }
}

void MPU6886Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPU6886:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MPU6886 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Acceleration X", this->accel_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->accel_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->accel_z_sensor_);
  LOG_SENSOR("  ", "Gyro X", this->gyro_x_sensor_);
  LOG_SENSOR("  ", "Gyro Y", this->gyro_y_sensor_);
  LOG_SENSOR("  ", "Gyro Z", this->gyro_z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

void MPU6886Component::update() {
  ESP_LOGV(TAG, "    Updating MPU6886...");
  uint16_t raw_data[7];
  if (!this->read_bytes_16(MPU6886_REGISTER_ACCEL_XOUT_H, raw_data, 7)) {
    this->status_set_warning();
    return;
  }
  auto *data = reinterpret_cast<int16_t *>(raw_data);

  float accel_x = data[0] * MPU6886_RANGE_PER_DIGIT_2G * GRAVITY_EARTH;
  float accel_y = data[1] * MPU6886_RANGE_PER_DIGIT_2G * GRAVITY_EARTH;
  float accel_z = data[2] * MPU6886_RANGE_PER_DIGIT_2G * GRAVITY_EARTH;

  float temperature = data[3] / TEMPERATURE_SENSITIVITY + TEMPERATURE_OFFSET;

  float gyro_x = data[4] * MPU6886_SCALE_DPS_PER_DIGIT_2000;
  float gyro_y = data[5] * MPU6886_SCALE_DPS_PER_DIGIT_2000;
  float gyro_z = data[6] * MPU6886_SCALE_DPS_PER_DIGIT_2000;

  ESP_LOGD(TAG,
           "Got accel={x=%.3f m/s², y=%.3f m/s², z=%.3f m/s²}, "
           "gyro={x=%.3f °/s, y=%.3f °/s, z=%.3f °/s}, temp=%.3f°C",
           accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, temperature);

  if (this->accel_x_sensor_ != nullptr)
    this->accel_x_sensor_->publish_state(accel_x);
  if (this->accel_y_sensor_ != nullptr)
    this->accel_y_sensor_->publish_state(accel_y);
  if (this->accel_z_sensor_ != nullptr)
    this->accel_z_sensor_->publish_state(accel_z);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);

  if (this->gyro_x_sensor_ != nullptr)
    this->gyro_x_sensor_->publish_state(gyro_x);
  if (this->gyro_y_sensor_ != nullptr)
    this->gyro_y_sensor_->publish_state(gyro_y);
  if (this->gyro_z_sensor_ != nullptr)
    this->gyro_z_sensor_->publish_state(gyro_z);

  this->status_clear_warning();
}

float MPU6886Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace mpu6886
}  // namespace esphome
