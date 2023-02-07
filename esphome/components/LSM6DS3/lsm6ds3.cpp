#include "lsm6ds3.h"
#include "esphome/core/log.h"
#include "constants.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace LSM6DS3 {
static const char *const TAG = "lsm6ds3";

void LSM6DS3Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LSM6DS3...");

  bool result = false;
  bool has_accel = false;

  /*
  If this component is to be power saving, and will
  sleep for more than 5 seconds, power off the device
  after each update call
  */
  if (this->_power_save) {
    this->write_byte(LSM6DS3_PERF_CTRL6_C, LSM6DS3_HIGH_PEF_DISABLE);
    if (this->update_interval_ > 5000) {
      this->do_sleep_ = true;
    }
  }

  if (this->high_perf_ == false) {
    this->write_byte(LSM6DS3_PERF_CTRL6_C, LSM6DS3_HIGH_PEF_DISABLE);
  }

  // Make sure there has been enough time for the device to initialize
  delay(20);

  if (this->accel_x_sensor_ != nullptr || this->accel_y_sensor_ != nullptr || this->accel_z_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "Setting up Acceleration");
    // accel bandwidth
    accl_conf_ |= LSM6DS3_ACC_GYRO_BW_XL_100Hz;
    // accel range
    accl_conf_ |= LSM6DS3_ACC_GYRO_FS_XL_16g;

    has_accel = true;
  }
  if (has_accel || this->temperature_sensor_ != nullptr) {
    this->has_accl_temp_ = true;
    switch (this->sample_accl_rate_) {
      default:
      case 13:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_13Hz;
        break;
      case 26:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_26Hz;
        break;
      case 52:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_52Hz;
        break;
      case 104:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_104Hz;
        break;
      case 208:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_208Hz;
        break;
      case 416:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_416Hz;
        break;
      case 833:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_833Hz;
        break;
      case 1660:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_1660Hz;
        break;
      case 3330:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_3330Hz;
        break;
      case 6660:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_6660Hz;
        break;
      case 13330:
        accl_conf_ |= LSM6DS3_ACC_GYRO_ODR_XL_13330Hz;
        break;
    }
  }
  result = this->write_byte(LSM6DS3_ACC_GYRO_CTRL1_XL, accl_conf_);
  if (!result) {
    this->mark_failed();
    return;
  }

  uint8_t data = 0;
  result = this->read_byte(LSM6DS3_ACC_GYRO_CTRL4_C, &data);
  if (!result) {
    this->mark_failed();
    return;
  }

  data &= ~((uint8_t) LSM6DS3_ACC_GYRO_BW_SCAL_ODR_ENABLED);
  data |= LSM6DS3_ACC_GYRO_BW_SCAL_ODR_ENABLED;

  result = this->write_byte(LSM6DS3_ACC_GYRO_CTRL4_C, data);
  if (!result) {
    this->mark_failed();
    return;
  }

  if (this->gyro_x_sensor_ != nullptr || this->gyro_y_sensor_ != nullptr || this->gyro_z_sensor_ != nullptr) {
    this->_has_gyro = true;
    ESP_LOGCONFIG(TAG, "Setting up Gyroscope");
    // gyro range
    gyro_conf_ |= LSM6DS3_ACC_GYRO_FS_G_2000dps;

    // gyro sampling rate
    switch (sample_gyro_rate_) {
      default:
      case 13:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_13Hz;
        break;
      case 26:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_26Hz;
        break;
      case 52:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_52Hz;
        break;
      case 104:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_104Hz;
        break;
      case 208:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_208Hz;
        break;
      case 416:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_416Hz;
        break;
      case 833:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_833Hz;
        break;
      case 1660:
        gyro_conf_ |= LSM6DS3_ACC_GYRO_ODR_G_1660Hz;
        break;
    }

    result = this->write_byte(LSM6DS3_ACC_GYRO_CTRL2_G, gyro_conf_);
    if (!result) {
      this->mark_failed();
      return;
    }
  }

  uint8_t who_am_i;
  this->read_byte(LSM6DS3_ACC_GYRO_WHO_AM_I_REG, &who_am_i);

  // Setup the internal temperature sensor
  if (this->temperature_sensor_ != nullptr) {
    if (who_am_i == LSM6DS3_ACC_GYRO_WHO_AM_I) {  // 0x69 LSM6DS3
      this->temp_sensitivity_ = 16;
    } else if (who_am_i == LSM6DS3_C_ACC_GYRO_WHO_AM_I) {  // 0x6A LSM6dS3-C
      this->temp_sensitivity_ = 256;
    }
  }

}

void LSM6DS3Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LSM6DS3:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with LSM6DS3 failed!");
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

void LSM6DS3Component::update() {
  ESP_LOGV(TAG, "    Updating LSM6DS3...");

  bool result;

  if (this->do_sleep_ && this->is_sleeping_) {
    this->is_sleeping_ = false;
    // Restore registers to their value to enable them
    if (has_accl_temp_) {
      result = this->write_byte(LSM6DS3_ACC_GYRO_CTRL1_XL, accl_conf_);
        if (!result) {
            ESP_LOGE(TAG, "Failed to come out of sleep state for acceleration sensor");
        }
    }
    if (_has_gyro) {
      result = this->write_byte(LSM6DS3_ACC_GYRO_CTRL2_G, gyro_conf_);
        if (!result) {
            ESP_LOGE(TAG, "Failed to come out of sleep state for gyro sensor");
        }
    }
    // give the unit time to wake;
    delay(20);
    if (has_accl_temp_) {
      // discard 4 readings to get correct value
      for (int i = 0; i < 4; i++) {
        // read accel
        this->_read_sensor(LSM6DS3_ACC_GYRO_OUTX_L_XL, this->accel_x_sensor_, SensorType::ACCEL, false);
        this->_read_sensor(LSM6DS3_ACC_GYRO_OUTY_L_XL, this->accel_y_sensor_, SensorType::ACCEL, false);
        this->_read_sensor(LSM6DS3_ACC_GYRO_OUTZ_L_XL, this->accel_z_sensor_, SensorType::ACCEL, false);

        // read temperature
        this->_read_sensor(LSM6DS3_ACC_GYRO_OUT_TEMP_L, this->temperature_sensor_, SensorType::TEMP, false);
      }
    }
    if (_has_gyro) {
      // discard 4 readings to get correct value
      for (int i = 0; i < 4; i++) {
        // read gyros
        this->_read_sensor(LSM6DS3_ACC_GYRO_OUTX_L_G, this->gyro_x_sensor_, SensorType::GYRO, false);
        this->_read_sensor(LSM6DS3_ACC_GYRO_OUTY_L_G, this->gyro_y_sensor_, SensorType::GYRO, false);
        this->_read_sensor(LSM6DS3_ACC_GYRO_OUTZ_L_G, this->gyro_z_sensor_, SensorType::GYRO, false);
      }
    }
  }

  // read accelerations
  this->_read_sensor(LSM6DS3_ACC_GYRO_OUTX_L_XL, this->accel_x_sensor_, SensorType::ACCEL, true);
  this->_read_sensor(LSM6DS3_ACC_GYRO_OUTY_L_XL, this->accel_y_sensor_, SensorType::ACCEL, true);
  this->_read_sensor(LSM6DS3_ACC_GYRO_OUTZ_L_XL, this->accel_z_sensor_, SensorType::ACCEL, true);

  // read gyros
  this->_read_sensor(LSM6DS3_ACC_GYRO_OUTX_L_G, this->gyro_x_sensor_, SensorType::GYRO, true);
  this->_read_sensor(LSM6DS3_ACC_GYRO_OUTY_L_G, this->gyro_y_sensor_, SensorType::GYRO, true);
  this->_read_sensor(LSM6DS3_ACC_GYRO_OUTZ_L_G, this->gyro_z_sensor_, SensorType::GYRO, true);

  // read temperature
  this->_read_sensor(LSM6DS3_ACC_GYRO_OUT_TEMP_L, this->temperature_sensor_, SensorType::TEMP, true);

  if (this->do_sleep_) {
    this->is_sleeping_ = true;
    // Set register(s) to zero to put them to sleep
    if (has_accl_temp_) {
      result = this->write_byte(LSM6DS3_ACC_GYRO_CTRL1_XL, 0x0);
        if (!result) {
            ESP_LOGE(TAG, "Failed to put acceleration/temp sensor to sleep");
        }
    }
    if (_has_gyro) {
      result = this->write_byte(LSM6DS3_ACC_GYRO_CTRL2_G, 0x0);
        if (!result) {
            ESP_LOGE(TAG, "Failed to put gyro sensor to sleep");
        }
    }
  }
}

void LSM6DS3Component::_read_sensor(uint8_t reg, sensor::Sensor *sensor, SensorType type, bool do_publish) {
  bool result = false;
  int16_t raw_value = 0;
  uint16_t buffer = 0;
  float output = 0;
  uint8_t gyro_range_divisor = 2000 / 125;
  int8_t tmp[2];

  if (sensor != nullptr) {
    result = this->read_byte_16(reg, &buffer);
    if (!result) {
      ESP_LOGE(TAG, "Could not read data from device");
    } else {
      // swap the byte order
      tmp[1] = (int8_t) buffer;
      tmp[0] = (int8_t)(buffer >> 8);
      raw_value = (int16_t) tmp[0] | ((int16_t) tmp[1]) << 8;
      switch (type) {
        case SensorType::ACCEL:
          output = (float) raw_value * 0.061 * (16 >> 1) / 1000;
          break;
        case SensorType::GYRO:
          output = (float) raw_value * 4.375 * (gyro_range_divisor) / 1000;
          break;
        case SensorType::TEMP:
          // add 25 to remove offset
          // scale by the sensitivity
          // result is in celsius
          output = (float) raw_value / this->temp_sensitivity_ + 25;
          break;
      }
      if (do_publish) {
        sensor->publish_state(output);
      }
    }
    delay(20);
  }
}

float LSM6DS3Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace LSM6DS3
}  // namespace esphome
