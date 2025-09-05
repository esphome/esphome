#include "qmi8658.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qmi8658 {

static const char *const TAG = "qmi8658";

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
    this->mark_failed();
    return;
  }

  if (!this->configure_accel_(this->accel_range_, this->accel_odr_, this->accel_lpf_mode_)) {
    this->mark_failed();
    return;
  }

  if (!this->configure_gyro_(this->gyro_range_, this->gyro_odr_, this->gyro_lpf_mode_)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up LPF config...");
  uint8_t lpf_config = 0;
  if (this->accel_lpf_mode_ != QMI8658LPFMode::QMI8658_LPF_OFF) {
    lpf_config |= (uint8_t) this->accel_lpf_mode_ << 1 | 1 << 0;
  }
  if (this->gyro_lpf_mode_ != QMI8658LPFMode::QMI8658_LPF_OFF) {
    lpf_config |= (uint8_t) this->gyro_lpf_mode_ << 5 | 1 << 4;
  }
  ESP_LOGV(TAG, "  lpf_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(lpf_config));
  if (!this->write_byte(QMI8658_REGISTER_CTRL5, lpf_config)) {
    ESP_LOGE(TAG, "Can't configure LPF");
    this->mark_failed();
    return;
  }

  if (!this->enable_required_sensors_()) {
    this->mark_failed();
    return;
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

  float temperature = (float) data[0] / 32768.f * 64.5f + 23.f;

  if (this->read_le_int16_(QMI8658_REGISTER_AX_L, data, 3) != i2c::ERROR_OK) {
    this->status_set_warning("Error reading acceleration data register");
    return;
  }

  float accel_x = (float) data[0] / (float) 32768.f * (1 << ((uint8_t) this->accel_range_ + 1)) * GRAVITY_EARTH;
  float accel_y = (float) data[1] / (float) 32768.f * (1 << ((uint8_t) this->accel_range_ + 1)) * GRAVITY_EARTH;
  float accel_z = (float) data[2] / (float) 32768.f * (1 << ((uint8_t) this->accel_range_ + 1)) * GRAVITY_EARTH;

  if (this->read_le_int16_(QMI8658_REGISTER_GX_L, data, 3) != i2c::ERROR_OK) {
    this->status_set_warning("Error reading gyroscope data register");
    return;
  }

  float gyro_x = (float) data[0] / (float) 32768.f * (1 << ((uint8_t) this->gyro_range_ + 4));
  float gyro_y = (float) data[1] / (float) 32768.f * (1 << ((uint8_t) this->gyro_range_ + 4));
  float gyro_z = (float) data[2] / (float) 32768.f * (1 << ((uint8_t) this->gyro_range_ + 4));

  ESP_LOGD(TAG,
           "Got accel={x=%.3f m/s², y=%.3f m/s², z=%.3f m/s²}, "
           "gyro={x=%.3f °/s, y=%.3f °/s, z=%.3f °/s}, temp=%.3f°C",
           accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, temperature);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);

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

  this->status_clear_warning();
}

bool QMI8658Component::clr_register_bit_(uint8_t reg, uint8_t bit) {
  uint8_t value;
  if (this->read_register(reg, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading register");
    return false;
  }
  value &= ~(1 << bit);
  if (this->write_register(reg, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing register");
    return false;
  }
  return true;
}

bool QMI8658Component::configure_accel_(QMI8658AccelRange accel_range, QMI8658AccelODR accel_odr,
                                        QMI8658LPFMode accel_lpf_mode) {
  ESP_LOGV(TAG, "  Setting up Accel Config...");
  uint8_t accel_config = (uint8_t) accel_range << 4 | (uint8_t) accel_odr;
  ESP_LOGV(TAG, "  accel_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_config));
  if (!this->write_byte(QMI8658_REGISTER_CTRL2, accel_config)) {
    ESP_LOGE(TAG, "Can't configure accelerometer");
    return false;
  }
  return true;
}

bool QMI8658Component::configure_gyro_(QMI8658GyroRange gyro_range, QMI8658GyroODR gyro_odr,
                                       QMI8658LPFMode gyro_lpf_mode) {
  ESP_LOGV(TAG, "  Setting up Gyro Config...");
  uint8_t gyro_config = (uint8_t) gyro_range_ << 4 | (uint8_t) gyro_odr_;
  ESP_LOGV(TAG, "  gyro_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_config));
  if (!this->write_byte(QMI8658_REGISTER_CTRL3, gyro_config)) {
    ESP_LOGE(TAG, "Can't configure gyroscope");
    return false;
  }
  return true;
}

bool QMI8658Component::disable_sensors_() {
  ESP_LOGV(TAG, "  Disabling sensors...");
  ESP_LOGV(TAG, "  sensors_state: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(QMI8658_SENSOR_NONE));
  return this->enable_sensors_(QMI8658_SENSOR_NONE);
}

bool QMI8658Component::disable_wake_on_motion() {
  if (!this->disable_sensors_()) {
    this->status_set_warning("Error disabling WoM: disabling sensors");
    return false;
  }

  ESP_LOGV(TAG, "  Disabling WoM threshold...");
  uint8_t threshold = 0x00;
  ESP_LOGV(TAG, "  threshold: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(threshold));
  if (this->write_register(QMI8658_REGISTER_CAL1_L, &threshold, 1) != i2c::ERROR_OK) {
    this->status_set_warning("Error disabling WoM: disabling threshold");
    return false;
  }

  ESP_LOGV(TAG, "  Saving WoM settings...");
  if (!this->send_command_(QMI8658_CMD_WRITE_WOM_SETTING)) {
    this->status_set_warning("Error disabling WoM: saving WoM settings");
    return false;
  }
  return this->enable_required_sensors_();
}

bool QMI8658Component::enable_required_sensors_() {
  ESP_LOGV(TAG, "  Enabling sensors...");
  uint8_t sensors_state = QMI8658_SENSOR_NONE;
  if (this->is_accel_required_())
    sensors_state |= QMI8658_SENSOR_ACCEL;
  if (this->is_gyro_required_())
    sensors_state |= QMI8658_SENSOR_GYRO;
  ESP_LOGV(TAG, "  sensors_state: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(sensors_state));
  if (!this->enable_sensors_(sensors_state)) {
    ESP_LOGE(TAG, "Can't enable required sensors");
    return false;
  }
  return true;
}

bool QMI8658Component::enable_sensors_(uint8_t sensors_state) {
  if (this->write_register(QMI8658_REGISTER_CTRL7, &sensors_state, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Can't enable/disable sensors");
    return false;
  }
  return true;
}

bool QMI8658Component::enable_wake_on_motion(uint8_t threshold, QMI8658AccelODR accel_odr,
                                             QMI8658InterruptPin interrupt_pin, uint8_t initial_pin_state,
                                             uint8_t blanking_time) {
  if (!this->disable_sensors_()) {
    this->status_set_warning("Error enabling WoM: disabling sensors");
    return false;
  }

  if (!this->configure_accel_(this->accel_range_, accel_odr, this->accel_lpf_mode_)) {
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

  ESP_LOGV(TAG, "  Saving WoM settings...");
  if (!this->send_command_(QMI8658_CMD_WRITE_WOM_SETTING)) {
    this->status_set_warning("Error enabling WoM: saving WoM settings");
    return false;
  }

  if (!this->enable_sensors_(QMI8658_SENSOR_ACCEL)) {
    ESP_LOGE(TAG, "Can't enable accelerometer");
    return false;
  }

  if (!this->set_register_bit_(QMI8658_REGISTER_CTRL1, (uint8_t) interrupt_pin + 3)) {
    ESP_LOGE(TAG, "Can't enable interrupt");
    return false;
  }

  return true;
}

i2c::ErrorCode QMI8658Component::read_le_int16_(uint8_t reg, int16_t *value, uint8_t len) {
  uint8_t raw_data[len * 2];
  i2c::ErrorCode err = this->read_register(reg, raw_data, len * 2);
  if (err != i2c::ERROR_OK)
    return err;
  for (int i = 0; i < len; i++) {
    value[i] = (int16_t) ((uint16_t) raw_data[i * 2] | ((uint16_t) raw_data[i * 2 + 1] << 8));
  }
  return err;
}

bool QMI8658Component::send_command_(uint8_t command, uint32_t wait_ms) {
  ESP_LOGV(TAG, "  Sending command...");
  ESP_LOGV(TAG, "  command: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(command));
  if (this->write_register(QMI8658_REGISTER_CTRL9, &command, 1) != i2c::ERROR_OK) {
    this->status_set_warning("Error sending command");
    return false;
  }

  uint32_t start_millis = millis();
  uint8_t val;
  do {
    if (this->read_register(QMI8658_REGISTER_STATUS_INT, &val, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Error reading command status register");
      return false;
    }
    delay(1);
    if (millis() - start_millis > wait_ms) {
      ESP_LOGE(TAG, "Command timed out");
      return false;
    }
  } while (!(val & 0x80));
  return true;
}

bool QMI8658Component::set_register_bit_(uint8_t reg, uint8_t bit) {
  uint8_t value;
  if (this->read_register(reg, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading register");
    return false;
  }
  value |= (1 << bit);
  if (this->write_register(reg, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing register");
    return false;
  }
  return true;
}

}  // namespace qmi8658
}  // namespace esphome
