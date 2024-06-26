#include "bmi160.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bmi160 {

static const char *const TAG = "bmi160";

const uint8_t BMI160_REGISTER_CHIPID = 0x00;

const uint8_t BMI160_REGISTER_CMD = 0x7E;
enum class Cmd : uint8_t {
  START_FOC = 0x03,
  ACCL_SET_PMU_MODE = 0b00010000,  // last 2 bits are mode
  GYRO_SET_PMU_MODE = 0b00010100,  // last 2 bits are mode
  MAG_SET_PMU_MODE = 0b00011000,   // last 2 bits are mode
  PROG_NVM = 0xA0,
  FIFO_FLUSH = 0xB0,
  INT_RESET = 0xB1,
  SOFT_RESET = 0xB6,
  STEP_CNT_CLR = 0xB2,
};
enum class GyroPmuMode : uint8_t {
  SUSPEND = 0b00,
  NORMAL = 0b01,
  LOW_POWER = 0b10,
};
enum class AcclPmuMode : uint8_t {
  SUSPEND = 0b00,
  NORMAL = 0b01,
  FAST_STARTUP = 0b11,
};
enum class MagPmuMode : uint8_t {
  SUSPEND = 0b00,
  NORMAL = 0b01,
  LOW_POWER = 0b10,
};

const uint8_t BMI160_REGISTER_ACCEL_CONFIG = 0x40;
enum class AcclFilterMode : uint8_t {
  POWER_SAVING = 0b00000000,
  PERF = 0b10000000,
};
enum class AcclBandwidth : uint8_t {
  OSR4_AVG1 = 0b00000000,
  OSR2_AVG2 = 0b00010000,
  NORMAL_AVG4 = 0b00100000,
  RES_AVG8 = 0b00110000,
  RES_AVG16 = 0b01000000,
  RES_AVG32 = 0b01010000,
  RES_AVG64 = 0b01100000,
  RES_AVG128 = 0b01110000,
};
enum class AccelOutputDataRate : uint8_t {
  HZ_25_32 = 0b0001,  // 25/32 Hz
  HZ_25_16 = 0b0010,  // 25/16 Hz
  HZ_25_8 = 0b0011,   // 25/8 Hz
  HZ_25_4 = 0b0100,   // 25/4 Hz
  HZ_25_2 = 0b0101,   // 25/2 Hz
  HZ_25 = 0b0110,     // 25 Hz
  HZ_50 = 0b0111,     // 50 Hz
  HZ_100 = 0b1000,    // 100 Hz
  HZ_200 = 0b1001,    // 200 Hz
  HZ_400 = 0b1010,    // 400 Hz
  HZ_800 = 0b1011,    // 800 Hz
  HZ_1600 = 0b1100,   // 1600 Hz
};
const uint8_t BMI160_REGISTER_ACCEL_RANGE = 0x41;
enum class AccelRange : uint8_t {
  RANGE_2G = 0b0011,
  RANGE_4G = 0b0101,
  RANGE_8G = 0b1000,
  RANGE_16G = 0b1100,
};

const uint8_t BMI160_REGISTER_GYRO_CONFIG = 0x42;
enum class GyroBandwidth : uint8_t {
  OSR4 = 0x00,
  OSR2 = 0x10,
  NORMAL = 0x20,
};
enum class GyroOuputDataRate : uint8_t {
  HZ_25 = 0x06,
  HZ_50 = 0x07,
  HZ_100 = 0x08,
  HZ_200 = 0x09,
  HZ_400 = 0x0A,
  HZ_800 = 0x0B,
  HZ_1600 = 0x0C,
  HZ_3200 = 0x0D,
};
const uint8_t BMI160_REGISTER_GYRO_RANGE = 0x43;
enum class GyroRange : uint8_t {
  RANGE_2000_DPS = 0x0,  // ±2000 °/s
  RANGE_1000_DPS = 0x1,
  RANGE_500_DPS = 0x2,
  RANGE_250_DPS = 0x3,
  RANGE_125_DPS = 0x4,
};

const uint8_t BMI160_REGISTER_DATA_GYRO_X_LSB = 0x0C;
const uint8_t BMI160_REGISTER_DATA_GYRO_X_MSB = 0x0D;
const uint8_t BMI160_REGISTER_DATA_GYRO_Y_LSB = 0x0E;
const uint8_t BMI160_REGISTER_DATA_GYRO_Y_MSB = 0x0F;
const uint8_t BMI160_REGISTER_DATA_GYRO_Z_LSB = 0x10;
const uint8_t BMI160_REGISTER_DATA_GYRO_Z_MSB = 0x11;
const uint8_t BMI160_REGISTER_DATA_ACCEL_X_LSB = 0x12;
const uint8_t BMI160_REGISTER_DATA_ACCEL_X_MSB = 0x13;
const uint8_t BMI160_REGISTER_DATA_ACCEL_Y_LSB = 0x14;
const uint8_t BMI160_REGISTER_DATA_ACCEL_Y_MSB = 0x15;
const uint8_t BMI160_REGISTER_DATA_ACCEL_Z_LSB = 0x16;
const uint8_t BMI160_REGISTER_DATA_ACCEL_Z_MSB = 0x17;
const uint8_t BMI160_REGISTER_DATA_TEMP_LSB = 0x20;
const uint8_t BMI160_REGISTER_DATA_TEMP_MSB = 0x21;

const float GRAVITY_EARTH = 9.80665f;

void BMI160Component::internal_setup_(int stage) {
  switch (stage) {
    case 0:
      ESP_LOGCONFIG(TAG, "Setting up BMI160...");
      uint8_t chipid;
      if (!this->read_byte(BMI160_REGISTER_CHIPID, &chipid) || (chipid != 0b11010001)) {
        this->mark_failed();
        return;
      }

      ESP_LOGV(TAG, "  Bringing accelerometer out of sleep...");
      if (!this->write_byte(BMI160_REGISTER_CMD, (uint8_t) Cmd::ACCL_SET_PMU_MODE | (uint8_t) AcclPmuMode::NORMAL)) {
        this->mark_failed();
        return;
      }
      ESP_LOGV(TAG, "  Waiting for accelerometer to wake up...");
      // need to wait (max delay in datasheet) because we can't send commands while another is in progress
      // min 5ms, 10ms
      this->set_timeout(10, [this]() { this->internal_setup_(1); });
      break;

    case 1:
      ESP_LOGV(TAG, "  Bringing gyroscope out of sleep...");
      if (!this->write_byte(BMI160_REGISTER_CMD, (uint8_t) Cmd::GYRO_SET_PMU_MODE | (uint8_t) GyroPmuMode::NORMAL)) {
        this->mark_failed();
        return;
      }
      ESP_LOGV(TAG, "  Waiting for gyroscope to wake up...");
      // wait between 51 & 81ms, doing 100 to be safe
      this->set_timeout(10, [this]() { this->internal_setup_(2); });
      break;

    case 2:
      ESP_LOGV(TAG, "  Setting up Gyro Config...");
      uint8_t gyro_config = (uint8_t) GyroBandwidth::OSR4 | (uint8_t) GyroOuputDataRate::HZ_25;
      ESP_LOGV(TAG, "  Output gyro_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_config));
      if (!this->write_byte(BMI160_REGISTER_GYRO_CONFIG, gyro_config)) {
        this->mark_failed();
        return;
      }
      ESP_LOGV(TAG, "  Setting up Gyro Range...");
      uint8_t gyro_range = (uint8_t) GyroRange::RANGE_2000_DPS;
      ESP_LOGV(TAG, "  Output gyro_range: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_range));
      if (!this->write_byte(BMI160_REGISTER_GYRO_RANGE, gyro_range)) {
        this->mark_failed();
        return;
      }

      ESP_LOGV(TAG, "  Setting up Accel Config...");
      uint8_t accel_config =
          (uint8_t) AcclFilterMode::PERF | (uint8_t) AcclBandwidth::RES_AVG16 | (uint8_t) AccelOutputDataRate::HZ_25;
      ESP_LOGV(TAG, "  Output accel_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_config));
      if (!this->write_byte(BMI160_REGISTER_ACCEL_CONFIG, accel_config)) {
        this->mark_failed();
        return;
      }
      ESP_LOGV(TAG, "  Setting up Accel Range...");
      uint8_t accel_range = (uint8_t) AccelRange::RANGE_16G;
      ESP_LOGV(TAG, "  Output accel_range: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_range));
      if (!this->write_byte(BMI160_REGISTER_ACCEL_RANGE, accel_range)) {
        this->mark_failed();
        return;
      }

      this->setup_complete_ = true;
  }
}

void BMI160Component::setup() { this->internal_setup_(0); }
void BMI160Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMI160:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BMI160 failed!");
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

i2c::ErrorCode BMI160Component::read_le_int16_(uint8_t reg, int16_t *value, uint8_t len) {
  uint8_t raw_data[len * 2];
  // read using read_register because we have little-endian data, and read_bytes_16 will swap it
  i2c::ErrorCode err = this->read_register(reg, raw_data, len * 2, true);
  if (err != i2c::ERROR_OK) {
    return err;
  }
  for (int i = 0; i < len; i++) {
    value[i] = (int16_t) ((uint16_t) raw_data[i * 2] | ((uint16_t) raw_data[i * 2 + 1] << 8));
  }
  return err;
}

void BMI160Component::update() {
  if (!this->setup_complete_) {
    return;
  }

  ESP_LOGV(TAG, "    Updating BMI160...");
  int16_t data[6];
  if (this->read_le_int16_(BMI160_REGISTER_DATA_GYRO_X_LSB, data, 6) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  float gyro_x = (float) data[0] / (float) INT16_MAX * 2000.f;
  float gyro_y = (float) data[1] / (float) INT16_MAX * 2000.f;
  float gyro_z = (float) data[2] / (float) INT16_MAX * 2000.f;
  float accel_x = (float) data[3] / (float) INT16_MAX * 16 * GRAVITY_EARTH;
  float accel_y = (float) data[4] / (float) INT16_MAX * 16 * GRAVITY_EARTH;
  float accel_z = (float) data[5] / (float) INT16_MAX * 16 * GRAVITY_EARTH;

  int16_t raw_temperature;
  if (this->read_le_int16_(BMI160_REGISTER_DATA_TEMP_LSB, &raw_temperature, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  float temperature = (float) raw_temperature / (float) INT16_MAX * 64.5f + 23.f;

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
float BMI160Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace bmi160
}  // namespace esphome
