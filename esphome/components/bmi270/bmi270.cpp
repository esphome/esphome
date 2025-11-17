#include "bmi270.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bmi270 {

static const char *const TAG = "bmi270";

const uint8_t BMI270_REGISTER_CHIP_ID = 0x00;
const uint8_t BMI270_CHIP_ID = 0x24;

const uint8_t BMI270_REGISTER_ERROR = 0x02;
const uint8_t BMI270_REGISTER_STATUS = 0x03;

// Data registers
const uint8_t BMI270_REGISTER_DATA_ACCEL_X_LSB = 0x0C;
const uint8_t BMI270_REGISTER_DATA_ACCEL_X_MSB = 0x0D;
const uint8_t BMI270_REGISTER_DATA_ACCEL_Y_LSB = 0x0E;
const uint8_t BMI270_REGISTER_DATA_ACCEL_Y_MSB = 0x0F;
const uint8_t BMI270_REGISTER_DATA_ACCEL_Z_LSB = 0x10;
const uint8_t BMI270_REGISTER_DATA_ACCEL_Z_MSB = 0x11;
const uint8_t BMI270_REGISTER_DATA_GYRO_X_LSB = 0x12;
const uint8_t BMI270_REGISTER_DATA_GYRO_X_MSB = 0x13;
const uint8_t BMI270_REGISTER_DATA_GYRO_Y_LSB = 0x14;
const uint8_t BMI270_REGISTER_DATA_GYRO_Y_MSB = 0x15;
const uint8_t BMI270_REGISTER_DATA_GYRO_Z_LSB = 0x16;
const uint8_t BMI270_REGISTER_DATA_GYRO_Z_MSB = 0x17;

const uint8_t BMI270_REGISTER_INTERNAL_STATUS = 0x21;
const uint8_t BMI270_REGISTER_TEMPERATURE_LSB = 0x22;
const uint8_t BMI270_REGISTER_TEMPERATURE_MSB = 0x23;

// Configuration registers
const uint8_t BMI270_REGISTER_ACC_CONF = 0x40;
const uint8_t BMI270_REGISTER_ACC_RANGE = 0x41;
const uint8_t BMI270_REGISTER_GYR_CONF = 0x42;
const uint8_t BMI270_REGISTER_GYR_RANGE = 0x43;

const uint8_t BMI270_REGISTER_INIT_CTRL = 0x59;
const uint8_t BMI270_REGISTER_INIT_ADDR_0 = 0x5B;
const uint8_t BMI270_REGISTER_INIT_ADDR_1 = 0x5C;
const uint8_t BMI270_REGISTER_INIT_DATA = 0x5E;

const uint8_t BMI270_REGISTER_PWR_CONF = 0x7C;
const uint8_t BMI270_REGISTER_PWR_CTRL = 0x7D;
const uint8_t BMI270_REGISTER_CMD = 0x7E;

// Commands
const uint8_t BMI270_CMD_SOFT_RESET = 0xB6;

// Power modes
const uint8_t BMI270_PWR_CONF_ADV_PWR_SAVE = 0x00;
const uint8_t BMI270_PWR_CTRL_ACC_ENABLE = 0x04;
const uint8_t BMI270_PWR_CTRL_GYR_ENABLE = 0x02;
const uint8_t BMI270_PWR_CTRL_TEMP_ENABLE = 0x08;

// Accelerometer configuration
const uint8_t BMI270_ACC_RANGE_2G = 0x00;
const uint8_t BMI270_ACC_RANGE_4G = 0x01;
const uint8_t BMI270_ACC_RANGE_8G = 0x02;
const uint8_t BMI270_ACC_RANGE_16G = 0x03;

// Gyroscope configuration
const uint8_t BMI270_GYR_RANGE_2000 = 0x00;
const uint8_t BMI270_GYR_RANGE_1000 = 0x01;
const uint8_t BMI270_GYR_RANGE_500 = 0x02;
const uint8_t BMI270_GYR_RANGE_250 = 0x03;
const uint8_t BMI270_GYR_RANGE_125 = 0x04;

// ODR (Output Data Rate) settings
const uint8_t BMI270_ACC_ODR_25HZ = 0x06;
const uint8_t BMI270_ACC_ODR_50HZ = 0x07;
const uint8_t BMI270_ACC_ODR_100HZ = 0x08;
const uint8_t BMI270_ACC_ODR_200HZ = 0x09;
const uint8_t BMI270_ACC_ODR_400HZ = 0x0A;
const uint8_t BMI270_ACC_ODR_800HZ = 0x0B;
const uint8_t BMI270_ACC_ODR_1600HZ = 0x0C;

const uint8_t BMI270_GYR_ODR_25HZ = 0x06;
const uint8_t BMI270_GYR_ODR_50HZ = 0x07;
const uint8_t BMI270_GYR_ODR_100HZ = 0x08;
const uint8_t BMI270_GYR_ODR_200HZ = 0x09;
const uint8_t BMI270_GYR_ODR_400HZ = 0x0A;
const uint8_t BMI270_GYR_ODR_800HZ = 0x0B;
const uint8_t BMI270_GYR_ODR_1600HZ = 0x0C;
const uint8_t BMI270_GYR_ODR_3200HZ = 0x0D;

// BWP (Bandwidth Parameter) settings
const uint8_t BMI270_ACC_BWP_NORMAL = 0x02;
const uint8_t BMI270_GYR_BWP_NORMAL = 0x02;

const float GRAVITY_EARTH = 9.80665f;

// BMI270 configuration file (reduced version for basic operation)
// This is a simplified config file - for full features, you would need the complete config from Bosch
static const uint8_t bmi270_config_file[] = {
    0xc8, 0x2e, 0x00, 0x2e, 0x80, 0x2e, 0x3d, 0xb1, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e,
    0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e,
    0x90, 0x32, 0x21, 0x2e, 0x59, 0xf5, 0x10, 0x30, 0x21, 0x2e, 0x6a, 0xf5, 0x1a, 0x24, 0x22, 0x00,
    0x80, 0x2e, 0x3b, 0xb1, 0x10, 0x30, 0x21, 0x2e, 0x6a, 0xf5, 0x80, 0x2e, 0x5a, 0xb1, 0x41, 0x33,
};

void BMI270Component::internal_setup_(SetupState state) {
  ESP_LOGCONFIG(TAG, "Setting up BMI270...");

  switch (state) {
    case SetupState::SOFT_RESET: {
      // Soft reset
      ESP_LOGV(TAG, "  Performing soft reset");
      if (!this->write_byte(BMI270_REGISTER_CMD, BMI270_CMD_SOFT_RESET)) {
        this->mark_failed("Soft reset failed");
        return;
      }
      // Wait for reset to complete
      this->set_timeout(10, [this]() { this->internal_setup_(SetupState::CHECK_CHIP_ID); });
      break;
    }

    case SetupState::CHECK_CHIP_ID: {
      // Check chip ID
      uint8_t chip_id;
      if (!this->read_byte(BMI270_REGISTER_CHIP_ID, &chip_id)) {
        this->mark_failed("Failed to read chip ID");
        return;
      }

      if (chip_id != BMI270_CHIP_ID) {
        this->mark_failed("Wrong chip ID");
        return;
      }

      ESP_LOGV(TAG, "  Chip ID verified: 0x%02X", chip_id);

      // Disable advanced power save mode for initialization
      if (!this->write_byte(BMI270_REGISTER_PWR_CONF, 0x00)) {
        this->mark_failed("Failed to disable advanced power save");
        return;
      }

      this->set_timeout(1, [this]() { this->internal_setup_(SetupState::PREPARE_CONFIG_UPLOAD); });
      break;
    }

    case SetupState::PREPARE_CONFIG_UPLOAD: {
      // Prepare for config file upload
      ESP_LOGV(TAG, "  Preparing to upload config file");

      // Disable loading of the configuration
      if (!this->write_byte(BMI270_REGISTER_INIT_CTRL, 0x00)) {
        this->mark_failed("Failed to prepare config upload");
        return;
      }

      this->set_timeout(1, [this]() { this->internal_setup_(SetupState::UPLOAD_CONFIG); });
      break;
    }

    case SetupState::UPLOAD_CONFIG: {
      // Upload configuration file in chunks
      ESP_LOGV(TAG, "  Uploading config file (%d bytes)", sizeof(bmi270_config_file));

      const size_t chunk_size = 16;
      const size_t total_size = sizeof(bmi270_config_file);

      for (size_t i = 0; i < total_size; i += chunk_size) {
        size_t bytes_to_write = (i + chunk_size > total_size) ? (total_size - i) : chunk_size;

        // Set address
        uint8_t addr_lsb = (i / 2) & 0xFF;
        uint8_t addr_msb = ((i / 2) >> 8) & 0xFF;

        if (!this->write_byte(BMI270_REGISTER_INIT_ADDR_0, addr_lsb)) {
          this->mark_failed("Failed to set config address LSB");
          return;
        }

        if (!this->write_byte(BMI270_REGISTER_INIT_ADDR_1, addr_msb)) {
          this->mark_failed("Failed to set config address MSB");
          return;
        }

        // Write data chunk
        if (!this->write_bytes(BMI270_REGISTER_INIT_DATA, &bmi270_config_file[i], bytes_to_write)) {
          this->mark_failed("Failed to write config data");
          return;
        }
      }

      ESP_LOGV(TAG, "  Config file uploaded successfully");
      this->set_timeout(10, [this]() { this->internal_setup_(SetupState::COMPLETE_INIT); });
      break;
    }

    case SetupState::COMPLETE_INIT: {
      // Complete initialization
      ESP_LOGV(TAG, "  Completing initialization");

      if (!this->write_byte(BMI270_REGISTER_INIT_CTRL, 0x01)) {
        this->mark_failed("Failed to complete initialization");
        return;
      }

      this->set_timeout(150, [this]() { this->internal_setup_(SetupState::CHECK_STATUS); });
      break;
    }

    case SetupState::CHECK_STATUS: {
      // Check internal status
      uint8_t internal_status;
      if (!this->read_byte(BMI270_REGISTER_INTERNAL_STATUS, &internal_status)) {
        this->mark_failed("Failed to read internal status");
        return;
      }

      ESP_LOGV(TAG, "  Internal status: 0x%02X", internal_status);

      if ((internal_status & 0x01) == 0) {
        this->mark_failed("Initialization failed - message not received");
        return;
      }

      // Enable accelerometer and gyroscope
      ESP_LOGV(TAG, "  Enabling accelerometer and gyroscope");
      uint8_t pwr_ctrl = BMI270_PWR_CTRL_ACC_ENABLE | BMI270_PWR_CTRL_GYR_ENABLE | BMI270_PWR_CTRL_TEMP_ENABLE;
      if (!this->write_byte(BMI270_REGISTER_PWR_CTRL, pwr_ctrl)) {
        this->mark_failed("Failed to enable sensors");
        return;
      }

      this->set_timeout(50, [this]() { this->internal_setup_(SetupState::CONFIGURE_SENSORS); });
      break;
    }

    case SetupState::CONFIGURE_SENSORS: {
      // Configure accelerometer
      ESP_LOGV(TAG, "  Configuring accelerometer");

      // Set range to ±16g
      if (!this->write_byte(BMI270_REGISTER_ACC_RANGE, BMI270_ACC_RANGE_16G)) {
        this->mark_failed("Failed to set accelerometer range");
        return;
      }

      // Set ODR to 100Hz, normal mode
      uint8_t acc_conf = (BMI270_ACC_BWP_NORMAL << 4) | BMI270_ACC_ODR_100HZ;
      if (!this->write_byte(BMI270_REGISTER_ACC_CONF, acc_conf)) {
        this->mark_failed("Failed to configure accelerometer");
        return;
      }

      // Configure gyroscope
      ESP_LOGV(TAG, "  Configuring gyroscope");

      // Set range to ±2000°/s
      if (!this->write_byte(BMI270_REGISTER_GYR_RANGE, BMI270_GYR_RANGE_2000)) {
        this->mark_failed("Failed to set gyroscope range");
        return;
      }

      // Set ODR to 100Hz, normal mode
      uint8_t gyr_conf = (BMI270_GYR_BWP_NORMAL << 4) | BMI270_GYR_ODR_100HZ;
      if (!this->write_byte(BMI270_REGISTER_GYR_CONF, gyr_conf)) {
        this->mark_failed("Failed to configure gyroscope");
        return;
      }

      ESP_LOGI(TAG, "BMI270 setup complete");
      this->setup_complete_ = true;
      break;
    }

    case SetupState::DONE:
      // Nothing to do
      break;
  }
}

void BMI270Component::setup() { this->internal_setup_(SetupState::SOFT_RESET); }

void BMI270Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMI270:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
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

i2c::ErrorCode BMI270Component::read_le_int16_(uint8_t reg, int16_t *value, uint8_t len) {
  uint8_t raw_data[len * 2];
  // read using read_register because we have little-endian data
  i2c::ErrorCode err = this->read_register(reg, raw_data, len * 2);
  if (err != i2c::ERROR_OK) {
    return err;
  }
  for (int i = 0; i < len; i++) {
    value[i] = (int16_t) ((uint16_t) raw_data[i * 2] | ((uint16_t) raw_data[i * 2 + 1] << 8));
  }
  return err;
}

void BMI270Component::update() {
  if (!this->setup_complete_) {
    return;
  }

  ESP_LOGV(TAG, "    Updating BMI270");

  // Read accelerometer data (6 bytes)
  int16_t accel_data[3];
  if (this->read_le_int16_(BMI270_REGISTER_DATA_ACCEL_X_LSB, accel_data, 3) != i2c::ERROR_OK) {
    this->status_set_warning("Failed to read accelerometer data");
    return;
  }

  // Read gyroscope data (6 bytes)
  int16_t gyro_data[3];
  if (this->read_le_int16_(BMI270_REGISTER_DATA_GYRO_X_LSB, gyro_data, 3) != i2c::ERROR_OK) {
    this->status_set_warning("Failed to read gyroscope data");
    return;
  }

  // Read temperature data (2 bytes)
  int16_t raw_temperature;
  if (this->read_le_int16_(BMI270_REGISTER_TEMPERATURE_LSB, &raw_temperature, 1) != i2c::ERROR_OK) {
    this->status_set_warning("Failed to read temperature data");
    return;
  }

  // Convert accelerometer data (±16g range)
  // LSB sensitivity: 16g range = 32768 LSB/16g = 2048 LSB/g
  float accel_x = (float) accel_data[0] / 2048.0f * GRAVITY_EARTH;
  float accel_y = (float) accel_data[1] / 2048.0f * GRAVITY_EARTH;
  float accel_z = (float) accel_data[2] / 2048.0f * GRAVITY_EARTH;

  // Convert gyroscope data (±2000°/s range)
  // LSB sensitivity: 2000°/s range = 32768 LSB/2000°/s = 16.384 LSB/(°/s)
  float gyro_x = (float) gyro_data[0] / 16.384f;
  float gyro_y = (float) gyro_data[1] / 16.384f;
  float gyro_z = (float) gyro_data[2] / 16.384f;

  // Convert temperature data
  // Temperature in °C = (raw_temp / 512) + 23
  float temperature = ((float) raw_temperature / 512.0f) + 23.0f;

  ESP_LOGD(TAG,
           "Got accel={x=%.3f m/s², y=%.3f m/s², z=%.3f m/s²}, "
           "gyro={x=%.3f °/s, y=%.3f °/s, z=%.3f °/s}, temp=%.1f°C",
           accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, temperature);

  if (this->accel_x_sensor_ != nullptr)
    this->accel_x_sensor_->publish_state(accel_x);
  if (this->accel_y_sensor_ != nullptr)
    this->accel_y_sensor_->publish_state(accel_y);
  if (this->accel_z_sensor_ != nullptr)
    this->accel_z_sensor_->publish_state(accel_z);

  if (this->gyro_x_sensor_ != nullptr)
    this->gyro_x_sensor_->publish_state(gyro_x);
  if (this->gyro_y_sensor_ != nullptr)
    this->gyro_y_sensor_->publish_state(gyro_y);
  if (this->gyro_z_sensor_ != nullptr)
    this->gyro_z_sensor_->publish_state(gyro_z);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);

  this->status_clear_warning();
}

float BMI270Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace bmi270
}  // namespace esphome
