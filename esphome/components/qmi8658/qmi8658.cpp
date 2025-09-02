#include "qmi8658.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qmi8658 {

static const char *const TAG = "qmi8658";


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

const uint8_t QMI8658_REGISTER_ACCEL_CONFIG = 0x40;
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

const uint8_t QMI8658_REGISTER_GYRO_CONFIG = 0x42;
enum class GyroBandwidth : uint8_t {
  OSR4 = 0x00,
  OSR2 = 0x10,
  NORMAL = 0x20,
};
enum class GyroOutputDataRate : uint8_t {
  HZ_25 = 0x06,
  HZ_50 = 0x07,
  HZ_100 = 0x08,
  HZ_200 = 0x09,
  HZ_400 = 0x0A,
  HZ_800 = 0x0B,
  HZ_1600 = 0x0C,
  HZ_3200 = 0x0D,
};
const uint8_t QMI8658_REGISTER_GYRO_RANGE = 0x43;
enum class GyroRange : uint8_t {
  RANGE_2000_DPS = 0x0,  // ±2000 °/s
  RANGE_1000_DPS = 0x1,
  RANGE_500_DPS = 0x2,
  RANGE_250_DPS = 0x3,
  RANGE_125_DPS = 0x4,
};

const uint8_t QMI8658_REGISTER_DATA_GYRO_X_LSB = 0x0C;
const uint8_t QMI8658_REGISTER_DATA_GYRO_X_MSB = 0x0D;
const uint8_t QMI8658_REGISTER_DATA_GYRO_Y_LSB = 0x0E;
const uint8_t QMI8658_REGISTER_DATA_GYRO_Y_MSB = 0x0F;
const uint8_t QMI8658_REGISTER_DATA_GYRO_Z_LSB = 0x10;
const uint8_t QMI8658_REGISTER_DATA_GYRO_Z_MSB = 0x11;
const uint8_t QMI8658_REGISTER_DATA_ACCEL_X_LSB = 0x12;
const uint8_t QMI8658_REGISTER_DATA_ACCEL_X_MSB = 0x13;
const uint8_t QMI8658_REGISTER_DATA_ACCEL_Y_LSB = 0x14;
const uint8_t QMI8658_REGISTER_DATA_ACCEL_Y_MSB = 0x15;
const uint8_t QMI8658_REGISTER_DATA_ACCEL_Z_LSB = 0x16;
const uint8_t QMI8658_REGISTER_DATA_ACCEL_Z_MSB = 0x17;
const uint8_t QMI8658_REGISTER_DATA_TEMP_LSB = 0x20;
const uint8_t QMI8658_REGISTER_DATA_TEMP_MSB = 0x21;

const float GRAVITY_EARTH = 9.80665f;

void QMI8658Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up QMI8658...");
  uint8_t chipid;
  if (this->read_byte(QMI8658_REGISTER_WHO_AM_I, &chipid)) {
    if (chipid != 0x05) {
      ESP_LOGE(TAG, "This is not a QMI8658 chip");
      this->mark_failed();
      return;
    }
  } else {
    ESP_LOGE(TAG, "Can't read WHO_AM_I register");
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up SPI Interface...");
  uint8_t spi_config = QMI8658_SPI_BE | QMI8658_SPI_AI;
  ESP_LOGV(TAG, "  spi_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(spi_config));
  if (!this->write_byte(QMI8658_REGISTER_CTRL1, spi_config)) {
    ESP_LOGE(TAG, "Can't configure SPI Interface");
    return this->mark_failed();
  }

  if (!this->configure_accel_(this->accel_range_, this->accel_odr_)) {
    return this->mark_failed();
  }

  ESP_LOGV(TAG, "  Setting up Gyro Config...");
  uint8_t gyro_config = (uint8_t) this->gyro_range_ << 4 | (uint8_t) this->gyro_odr_;
  ESP_LOGV(TAG, "  gyro_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_config));
  if (!this->write_byte(QMI8658_REGISTER_CTRL3, gyro_config)) {
    ESP_LOGE(TAG, "Can't configure gyroscope");
    return this->mark_failed();
  }

  
}

void QMI8658Component::dump_config() {
  ESP_LOGCONFIG(TAG, "QMI8658:");
  ESP_LOGCONFIG(TAG, "    Acceleration ODR: %u", this->accel_odr_);
  ESP_LOGCONFIG(TAG, "    Acceleration range: %u", this->accel_range_);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with QMI8658 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Acceleration X", this->accel_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->accel_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->accel_z_sensor_);
  LOG_SENSOR("  ", "Gyroscope X", this->gyro_x_sensor_);
  LOG_SENSOR("  ", "Gyroscope Y", this->gyro_y_sensor_);
  LOG_SENSOR("  ", "Gyroscope Z", this->gyro_z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

void QMI8658Component::update() {
  ESP_LOGV(TAG, "    Updating QMI8658...");
  int16_t data[3];

  if (this->read_le_int16_(QMI8658_REGISTER_TEMP_L, data, 1) != i2c::ERROR_OK) {
    this->status_set_warning("Error reading temperature data register");
    return;
  }

  float temperature = (float) data[0] / (float) INT16_MAX * 64.5f + 23.f;


  if (this->read_le_int16_(QMI8658_REGISTER_AX_L, data, 3) != i2c::ERROR_OK) {
    this->status_set_warning("Error reading acceleration data register");
    return;
  }

  float accel_x = (float) data[0] / (float) INT16_MAX * (1 << (uint8_t) this->accel_range_ + 1) * GRAVITY_EARTH;
  float accel_y = (float) data[1] / (float) INT16_MAX * (1 << (uint8_t) this->accel_range_ + 1) * GRAVITY_EARTH;
  float accel_z = (float) data[2] / (float) INT16_MAX * (1 << (uint8_t) this->accel_range_ + 1) * GRAVITY_EARTH;
  
  if (this->read_le_int16_(QMI8658_REGISTER_GX_L, data, 3) != i2c::ERROR_OK) {
    this->status_set_warning("Error reading gyroscope data register");
    return;
  }
  
  float gyro_x = (float) data[0] / (float) INT16_MAX * (1 << (uint8_t) this->gyro_range_ + 4);
  float gyro_y = (float) data[1] / (float) INT16_MAX * (1 << (uint8_t) this->gyro_range_ + 4);
  float gyro_z = (float) data[2] / (float) INT16_MAX * (1 << (uint8_t) this->gyro_range_ + 4);

  ESP_LOGD(TAG,
           "Got accel={x=%.3f m/s², y=%.3f m/s², z=%.3f m/s²}, "
           "gyro={x=%.3f °/s, y=%.3f °/s, z=%.3f °/s}, temp=%.3f°C",
           accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, temperature);

  if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state(temperature);

  if (this->accel_x_sensor_ != nullptr) this->accel_x_sensor_->publish_state(accel_x);
  if (this->accel_y_sensor_ != nullptr) this->accel_y_sensor_->publish_state(accel_y);
  if (this->accel_z_sensor_ != nullptr) this->accel_z_sensor_->publish_state(accel_z);

  if (this->gyro_x_sensor_ != nullptr) this->gyro_x_sensor_->publish_state(gyro_x);
  if (this->gyro_y_sensor_ != nullptr) this->gyro_y_sensor_->publish_state(gyro_y);
  if (this->gyro_z_sensor_ != nullptr) this->gyro_z_sensor_->publish_state(gyro_z);

  this->status_clear_warning();
}

bool QMI8658Component::configure_accel_(QMI8658AccelRange accel_range, QMI8658AccelODR accel_odr) {
  ESP_LOGV(TAG, "  Setting up Accel Config...");
  uint8_t accel_config = (uint8_t) accel_range << 4 | (uint8_t) accel_odr;
  ESP_LOGV(TAG, "  accel_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_config));
  if (!this->write_byte(QMI8658_REGISTER_CTRL2, accel_config)) {
    ESP_LOGE(TAG, "Can't configure accelerometer");
    return false;
  }
  return true;
}

bool QMI8658Component::disable_sensors_() {
  ESP_LOGV(TAG, "  Disabling sensors...");
  ESP_LOGV(TAG, "  sensors_state: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(QMI8658_SENSOR_NONE));
  return this->enable_sensors_(QMI8658_SENSOR_NONE);
}

bool QMI8658Component::enable_required_sensors_() {
  ESP_LOGV(TAG, "  Enabling sensors...");
  uint8_t sensors_state = QMI8658_SENSOR_NONE;
  if (this->is_accel_required_()) sensors_state |= QMI8658_SENSOR_ACCEL;
  if (this->is_gyro_required_()) sensors_state |= QMI8658_SENSOR_GYRO;
  ESP_LOGV(TAG, "  sensors_state: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(sensors_state));
  return this->enable_sensors_(sensors_state);
}

bool QMI8658Component::enable_sensors_(uint8_t sensors_state) {
  if (this->write_register(QMI8658_REGISTER_CTRL7, &sensors_state, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Can't enable/disable sensors");
    return false;
  }
  return true;
}

bool QMI8658Component::enable_wake_on_motion(uint8_t threshold, QMI8658AccelRange accel_range, QMI8658AccelODR accel_odr, QMI8658InterruptPin interrupt_pin, uint8_t initial_pin_state, uint8_t blanking_time) {
  if (!this->disable_sensors_()) {
    this->status_set_warning("Error enabling WoM: disabling sensors");
    return false;
  }
  if (!this->configure_accel_(accel_range, accel_odr)) {
    this->status_set_warning("Error enabling WoM: configuring accel");
    return false;
  }
  
  ESP_LOGV(TAG, "  Configuring WoM threshold...");
  ESP_LOGV(TAG, "  threshold: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(threshold));
  if (this->write_register(QMI8658_REGISTER_CAL1_L, &threshold, 1) != i2c::ERROR_OK) {
    this->status_set_warning("Error enabling WoM: configuring threshold");
    return false;
  }
  
  ESP_LOGV(TAG, "  Configuring WoM interrupt pin...");
  uint8_t interrupt_pin_config = initial_pin_state << 7 | (uint8_t) interrupt_pin << 6 | (blanking_time & 0x3F);
  ESP_LOGV(TAG, "  interrupt_pin_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(interrupt_pin_config));
  if (this->write_register(QMI8658_REGISTER_CAL1_H, &interrupt_pin_config, 1) != i2c::ERROR_OK) {
    this->status_set_warning("Error enabling WoM: configuring interrupt pin");
    return false;
  }

  ESP_LOGV(TAG, "  Enabling WoM...");
  if (!this->send_command_(QMI8658_CMD_WRITE_WOM_SETTING)) {
    this->status_set_warning("Error enabling WoM: sending command");
    return false;
  }
  
  if (!this->enable_sensors_(QMI8658_SENSOR_ACCEL)) {
    ESP_LOGE(TAG, "Can't enable accelerometer");
    return false;
  }
  return true;
}

i2c::ErrorCode QMI8658Component::read_le_int16_(uint8_t reg, int16_t *value, uint8_t len) {
  uint8_t raw_data[len * 2];
  i2c::ErrorCode err = this->read_register(reg, raw_data, len * 2, true);
  if (err != i2c::ERROR_OK) return err;
  for (int i = 0; i < len; i++) {
    value[i] = (int16_t) ((uint16_t) raw_data[i * 2] | ((uint16_t) raw_data[i * 2 + 1] << 8));
  }
  return err;
}

bool QMI8658Component::send_command_(uint8_t command) {
  ESP_LOGV(TAG, "  Sending command...");
  ESP_LOGV(TAG, "  command: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(command));
  if (this->write_register(QMI8658_REGISTER_CTRL9, &command, 1) != i2c::ERROR_OK) {
    this->status_set_warning("Error sending command");
    return false;
  }
  return true;
}

}  // namespace qmi8658
}  // namespace esphome
