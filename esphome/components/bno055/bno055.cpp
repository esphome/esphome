#include "bno055.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bno055 {

static const char *const TAG = "bno055";

void BNO055Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BNO055...");

  // Check chip ID
  uint8_t chip_id;
  if (!this->read_byte(BNO055_REGISTER_CHIP_ID, &chip_id)) {
    ESP_LOGE(TAG, "Failed to read chip ID");
    this->mark_failed();
    return;
  }

  if (chip_id != BNO055_CHIP_ID) {
    ESP_LOGE(TAG, "Invalid chip ID: 0x%02X (expected 0x%02X)", chip_id, BNO055_CHIP_ID);
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "Chip ID: 0x%02X", chip_id);

  // Switch to config mode for setup
  if (!this->set_mode_(OPERATION_MODE_CONFIG)) {
    ESP_LOGE(TAG, "Failed to set config mode");
    this->mark_failed();
    return;
  }

  // Reset the device
  if (!this->write_byte(BNO055_REGISTER_SYS_TRIGGER, 0x20)) {
    ESP_LOGE(TAG, "Failed to reset device");
    this->mark_failed();
    return;
  }

  // Wait for reset to complete (BNO055 needs ~650ms after reset)
  delay(700);  // NOLINT

  // Wait for chip ID to be readable again after reset
  uint8_t attempts = 0;
  while (attempts < 10) {
    if (this->read_byte(BNO055_REGISTER_CHIP_ID, &chip_id) && chip_id == BNO055_CHIP_ID) {
      break;
    }
    delay(50);  // NOLINT
    attempts++;
  }

  if (chip_id != BNO055_CHIP_ID) {
    ESP_LOGE(TAG, "Chip not responding after reset");
    this->mark_failed();
    return;
  }

  // Set to normal power mode
  if (!this->write_byte(BNO055_REGISTER_PWR_MODE, POWER_MODE_NORMAL)) {
    ESP_LOGE(TAG, "Failed to set power mode");
    this->mark_failed();
    return;
  }
  delay(10);

  // Set page to 0
  if (!this->write_byte(BNO055_REGISTER_PAGE_ID, 0x00)) {
    ESP_LOGE(TAG, "Failed to set page");
    this->mark_failed();
    return;
  }

  // Clear system trigger
  if (!this->write_byte(BNO055_REGISTER_SYS_TRIGGER, 0x00)) {
    ESP_LOGE(TAG, "Failed to clear system trigger");
    this->mark_failed();
    return;
  }
  delay(10);

  // Set to NDOF mode (9-DOF sensor fusion)
  if (!this->set_mode_(OPERATION_MODE_NDOF)) {
    ESP_LOGE(TAG, "Failed to set NDOF mode");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "BNO055 initialized successfully");
}

bool BNO055Component::set_mode_(BNO055OperationMode mode) {
  if (!this->write_byte(BNO055_REGISTER_OPR_MODE, mode)) {
    return false;
  }
  // Mode switch delay: to/from config mode takes 19ms, others take 7ms
  delay(mode == OPERATION_MODE_CONFIG ? 25 : 10);
  return true;
}

void BNO055Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BNO055:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Euler Heading", this->euler_heading_sensor_);
  LOG_SENSOR("  ", "Euler Roll", this->euler_roll_sensor_);
  LOG_SENSOR("  ", "Euler Pitch", this->euler_pitch_sensor_);
  LOG_SENSOR("  ", "Quaternion W", this->quaternion_w_sensor_);
  LOG_SENSOR("  ", "Quaternion X", this->quaternion_x_sensor_);
  LOG_SENSOR("  ", "Quaternion Y", this->quaternion_y_sensor_);
  LOG_SENSOR("  ", "Quaternion Z", this->quaternion_z_sensor_);
  LOG_SENSOR("  ", "Linear Accel X", this->linear_accel_x_sensor_);
  LOG_SENSOR("  ", "Linear Accel Y", this->linear_accel_y_sensor_);
  LOG_SENSOR("  ", "Linear Accel Z", this->linear_accel_z_sensor_);
  LOG_SENSOR("  ", "Gravity X", this->gravity_x_sensor_);
  LOG_SENSOR("  ", "Gravity Y", this->gravity_y_sensor_);
  LOG_SENSOR("  ", "Gravity Z", this->gravity_z_sensor_);
  LOG_SENSOR("  ", "Acceleration X", this->accel_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->accel_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->accel_z_sensor_);
  LOG_SENSOR("  ", "Gyro X", this->gyro_x_sensor_);
  LOG_SENSOR("  ", "Gyro Y", this->gyro_y_sensor_);
  LOG_SENSOR("  ", "Gyro Z", this->gyro_z_sensor_);
  LOG_SENSOR("  ", "Magnetometer X", this->mag_x_sensor_);
  LOG_SENSOR("  ", "Magnetometer Y", this->mag_y_sensor_);
  LOG_SENSOR("  ", "Magnetometer Z", this->mag_z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

void BNO055Component::update() {
  ESP_LOGV(TAG, "Updating BNO055 sensors");

  // Read Euler angles (6 bytes: heading, roll, pitch)
  if (this->euler_heading_sensor_ != nullptr || this->euler_roll_sensor_ != nullptr ||
      this->euler_pitch_sensor_ != nullptr) {
    uint8_t euler_data[6];
    if (this->read_bytes(BNO055_REGISTER_EULER_H_LSB, euler_data, 6)) {
      int16_t heading_raw = (int16_t) ((euler_data[1] << 8) | euler_data[0]);
      int16_t roll_raw = (int16_t) ((euler_data[3] << 8) | euler_data[2]);
      int16_t pitch_raw = (int16_t) ((euler_data[5] << 8) | euler_data[4]);

      float heading = heading_raw * BNO055_EULER_SCALE;
      float roll = roll_raw * BNO055_EULER_SCALE;
      float pitch = pitch_raw * BNO055_EULER_SCALE;

      ESP_LOGD(TAG, "Euler: heading=%.1f°, roll=%.1f°, pitch=%.1f°", heading, roll, pitch);

      if (this->euler_heading_sensor_ != nullptr)
        this->euler_heading_sensor_->publish_state(heading);
      if (this->euler_roll_sensor_ != nullptr)
        this->euler_roll_sensor_->publish_state(roll);
      if (this->euler_pitch_sensor_ != nullptr)
        this->euler_pitch_sensor_->publish_state(pitch);
    } else {
      ESP_LOGW(TAG, "Failed to read Euler angles");
      this->status_set_warning();
    }
  }

  // Read Quaternion (8 bytes: w, x, y, z)
  if (this->quaternion_w_sensor_ != nullptr || this->quaternion_x_sensor_ != nullptr ||
      this->quaternion_y_sensor_ != nullptr || this->quaternion_z_sensor_ != nullptr) {
    uint8_t quat_data[8];
    if (this->read_bytes(BNO055_REGISTER_QUATERNION_W_LSB, quat_data, 8)) {
      int16_t w_raw = (int16_t) ((quat_data[1] << 8) | quat_data[0]);
      int16_t x_raw = (int16_t) ((quat_data[3] << 8) | quat_data[2]);
      int16_t y_raw = (int16_t) ((quat_data[5] << 8) | quat_data[4]);
      int16_t z_raw = (int16_t) ((quat_data[7] << 8) | quat_data[6]);

      float w = w_raw * BNO055_QUATERNION_SCALE;
      float x = x_raw * BNO055_QUATERNION_SCALE;
      float y = y_raw * BNO055_QUATERNION_SCALE;
      float z = z_raw * BNO055_QUATERNION_SCALE;

      ESP_LOGD(TAG, "Quaternion: w=%.4f, x=%.4f, y=%.4f, z=%.4f", w, x, y, z);

      if (this->quaternion_w_sensor_ != nullptr)
        this->quaternion_w_sensor_->publish_state(w);
      if (this->quaternion_x_sensor_ != nullptr)
        this->quaternion_x_sensor_->publish_state(x);
      if (this->quaternion_y_sensor_ != nullptr)
        this->quaternion_y_sensor_->publish_state(y);
      if (this->quaternion_z_sensor_ != nullptr)
        this->quaternion_z_sensor_->publish_state(z);
    } else {
      ESP_LOGW(TAG, "Failed to read quaternion");
      this->status_set_warning();
    }
  }

  // Read Linear Acceleration (6 bytes: x, y, z)
  if (this->linear_accel_x_sensor_ != nullptr || this->linear_accel_y_sensor_ != nullptr ||
      this->linear_accel_z_sensor_ != nullptr) {
    uint8_t lin_accel_data[6];
    if (this->read_bytes(BNO055_REGISTER_LINEAR_ACCEL_X_LSB, lin_accel_data, 6)) {
      int16_t x_raw = (int16_t) ((lin_accel_data[1] << 8) | lin_accel_data[0]);
      int16_t y_raw = (int16_t) ((lin_accel_data[3] << 8) | lin_accel_data[2]);
      int16_t z_raw = (int16_t) ((lin_accel_data[5] << 8) | lin_accel_data[4]);

      float x = x_raw * BNO055_ACCEL_SCALE;
      float y = y_raw * BNO055_ACCEL_SCALE;
      float z = z_raw * BNO055_ACCEL_SCALE;

      ESP_LOGD(TAG, "Linear Accel: x=%.2f m/s², y=%.2f m/s², z=%.2f m/s²", x, y, z);

      if (this->linear_accel_x_sensor_ != nullptr)
        this->linear_accel_x_sensor_->publish_state(x);
      if (this->linear_accel_y_sensor_ != nullptr)
        this->linear_accel_y_sensor_->publish_state(y);
      if (this->linear_accel_z_sensor_ != nullptr)
        this->linear_accel_z_sensor_->publish_state(z);
    } else {
      ESP_LOGW(TAG, "Failed to read linear acceleration");
      this->status_set_warning();
    }
  }

  // Read Gravity Vector (6 bytes: x, y, z)
  if (this->gravity_x_sensor_ != nullptr || this->gravity_y_sensor_ != nullptr || this->gravity_z_sensor_ != nullptr) {
    uint8_t gravity_data[6];
    if (this->read_bytes(BNO055_REGISTER_GRAVITY_X_LSB, gravity_data, 6)) {
      int16_t x_raw = (int16_t) ((gravity_data[1] << 8) | gravity_data[0]);
      int16_t y_raw = (int16_t) ((gravity_data[3] << 8) | gravity_data[2]);
      int16_t z_raw = (int16_t) ((gravity_data[5] << 8) | gravity_data[4]);

      float x = x_raw * BNO055_ACCEL_SCALE;
      float y = y_raw * BNO055_ACCEL_SCALE;
      float z = z_raw * BNO055_ACCEL_SCALE;

      ESP_LOGD(TAG, "Gravity: x=%.2f m/s², y=%.2f m/s², z=%.2f m/s²", x, y, z);

      if (this->gravity_x_sensor_ != nullptr)
        this->gravity_x_sensor_->publish_state(x);
      if (this->gravity_y_sensor_ != nullptr)
        this->gravity_y_sensor_->publish_state(y);
      if (this->gravity_z_sensor_ != nullptr)
        this->gravity_z_sensor_->publish_state(z);
    } else {
      ESP_LOGW(TAG, "Failed to read gravity vector");
      this->status_set_warning();
    }
  }

  // Read Raw Accelerometer (6 bytes: x, y, z)
  if (this->accel_x_sensor_ != nullptr || this->accel_y_sensor_ != nullptr || this->accel_z_sensor_ != nullptr) {
    uint8_t accel_data[6];
    if (this->read_bytes(BNO055_REGISTER_ACCEL_DATA_X_LSB, accel_data, 6)) {
      int16_t x_raw = (int16_t) ((accel_data[1] << 8) | accel_data[0]);
      int16_t y_raw = (int16_t) ((accel_data[3] << 8) | accel_data[2]);
      int16_t z_raw = (int16_t) ((accel_data[5] << 8) | accel_data[4]);

      float x = x_raw * BNO055_ACCEL_SCALE;
      float y = y_raw * BNO055_ACCEL_SCALE;
      float z = z_raw * BNO055_ACCEL_SCALE;

      ESP_LOGD(TAG, "Accel: x=%.2f m/s², y=%.2f m/s², z=%.2f m/s²", x, y, z);

      if (this->accel_x_sensor_ != nullptr)
        this->accel_x_sensor_->publish_state(x);
      if (this->accel_y_sensor_ != nullptr)
        this->accel_y_sensor_->publish_state(y);
      if (this->accel_z_sensor_ != nullptr)
        this->accel_z_sensor_->publish_state(z);
    } else {
      ESP_LOGW(TAG, "Failed to read accelerometer");
      this->status_set_warning();
    }
  }

  // Read Gyroscope (6 bytes: x, y, z)
  if (this->gyro_x_sensor_ != nullptr || this->gyro_y_sensor_ != nullptr || this->gyro_z_sensor_ != nullptr) {
    uint8_t gyro_data[6];
    if (this->read_bytes(BNO055_REGISTER_GYRO_DATA_X_LSB, gyro_data, 6)) {
      int16_t x_raw = (int16_t) ((gyro_data[1] << 8) | gyro_data[0]);
      int16_t y_raw = (int16_t) ((gyro_data[3] << 8) | gyro_data[2]);
      int16_t z_raw = (int16_t) ((gyro_data[5] << 8) | gyro_data[4]);

      float x = x_raw * BNO055_GYRO_SCALE;
      float y = y_raw * BNO055_GYRO_SCALE;
      float z = z_raw * BNO055_GYRO_SCALE;

      ESP_LOGD(TAG, "Gyro: x=%.2f °/s, y=%.2f °/s, z=%.2f °/s", x, y, z);

      if (this->gyro_x_sensor_ != nullptr)
        this->gyro_x_sensor_->publish_state(x);
      if (this->gyro_y_sensor_ != nullptr)
        this->gyro_y_sensor_->publish_state(y);
      if (this->gyro_z_sensor_ != nullptr)
        this->gyro_z_sensor_->publish_state(z);
    } else {
      ESP_LOGW(TAG, "Failed to read gyroscope");
      this->status_set_warning();
    }
  }

  // Read Magnetometer (6 bytes: x, y, z)
  if (this->mag_x_sensor_ != nullptr || this->mag_y_sensor_ != nullptr || this->mag_z_sensor_ != nullptr) {
    uint8_t mag_data[6];
    if (this->read_bytes(BNO055_REGISTER_MAG_DATA_X_LSB, mag_data, 6)) {
      int16_t x_raw = (int16_t) ((mag_data[1] << 8) | mag_data[0]);
      int16_t y_raw = (int16_t) ((mag_data[3] << 8) | mag_data[2]);
      int16_t z_raw = (int16_t) ((mag_data[5] << 8) | mag_data[4]);

      float x = x_raw * BNO055_MAG_SCALE;
      float y = y_raw * BNO055_MAG_SCALE;
      float z = z_raw * BNO055_MAG_SCALE;

      ESP_LOGD(TAG, "Mag: x=%.1f µT, y=%.1f µT, z=%.1f µT", x, y, z);

      if (this->mag_x_sensor_ != nullptr)
        this->mag_x_sensor_->publish_state(x);
      if (this->mag_y_sensor_ != nullptr)
        this->mag_y_sensor_->publish_state(y);
      if (this->mag_z_sensor_ != nullptr)
        this->mag_z_sensor_->publish_state(z);
    } else {
      ESP_LOGW(TAG, "Failed to read magnetometer");
      this->status_set_warning();
    }
  }

  // Read Temperature (1 byte)
  if (this->temperature_sensor_ != nullptr) {
    uint8_t temp_data;
    if (this->read_byte(BNO055_REGISTER_TEMP, &temp_data)) {
      // Temperature is a signed 8-bit value in Celsius
      int8_t temp = static_cast<int8_t>(temp_data);
      ESP_LOGD(TAG, "Temperature: %d °C", temp);
      this->temperature_sensor_->publish_state(temp);
    } else {
      ESP_LOGW(TAG, "Failed to read temperature");
      this->status_set_warning();
    }
  }

  this->status_clear_warning();
}

float BNO055Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace bno055
}  // namespace esphome
