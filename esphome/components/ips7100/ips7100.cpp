#include "ips7100.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ips7100 {

static const char *const TAG = "ips7100";

// I2C commands
static const uint8_t IPS7100_CMD_READ_PC = 0x11;   // Read particle count data (30 bytes)
static const uint8_t IPS7100_CMD_READ_PM = 0x12;   // Read PM mass data (32 bytes)

// Data sizes
static const uint8_t PC_DATA_SIZE = 30;   // 7 x 4 bytes + 2 bytes checksum
static const uint8_t PM_DATA_SIZE = 32;   // 7 x 4 bytes + 2 bytes event status + 2 bytes checksum

// CRC16 polynomial
static const uint16_t CRC16_POLYNOMIAL = 0x8408;

void IPS7100Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up IPS7100...");

  // Try to read data to verify sensor is responding
  if (!this->read_pm_data_()) {
    ESP_LOGE(TAG, "Failed to communicate with IPS7100 sensor");
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "IPS7100 sensor initialized successfully");
}

void IPS7100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "IPS7100:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "PM 0.1", this->pm_0_1_sensor_);
  LOG_SENSOR("  ", "PM 0.3", this->pm_0_3_sensor_);
  LOG_SENSOR("  ", "PM 0.5", this->pm_0_5_sensor_);
  LOG_SENSOR("  ", "PM 1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM 2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM 5.0", this->pm_5_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);

  LOG_SENSOR("  ", "PMC 0.1", this->pmc_0_1_sensor_);
  LOG_SENSOR("  ", "PMC 0.3", this->pmc_0_3_sensor_);
  LOG_SENSOR("  ", "PMC 0.5", this->pmc_0_5_sensor_);
  LOG_SENSOR("  ", "PMC 1.0", this->pmc_1_0_sensor_);
  LOG_SENSOR("  ", "PMC 2.5", this->pmc_2_5_sensor_);
  LOG_SENSOR("  ", "PMC 5.0", this->pmc_5_0_sensor_);
  LOG_SENSOR("  ", "PMC 10.0", this->pmc_10_0_sensor_);
}

void IPS7100Component::update() {
  bool pm_success = this->read_pm_data_();
  bool pc_success = this->read_pc_data_();

  if (!pm_success && !pc_success) {
    ESP_LOGW(TAG, "Failed to read data from IPS7100");
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Publish PM mass values
  if (pm_success) {
    if (this->pm_0_1_sensor_ != nullptr)
      this->pm_0_1_sensor_->publish_state(this->pm_values_[0]);
    if (this->pm_0_3_sensor_ != nullptr)
      this->pm_0_3_sensor_->publish_state(this->pm_values_[1]);
    if (this->pm_0_5_sensor_ != nullptr)
      this->pm_0_5_sensor_->publish_state(this->pm_values_[2]);
    if (this->pm_1_0_sensor_ != nullptr)
      this->pm_1_0_sensor_->publish_state(this->pm_values_[3]);
    if (this->pm_2_5_sensor_ != nullptr)
      this->pm_2_5_sensor_->publish_state(this->pm_values_[4]);
    if (this->pm_5_0_sensor_ != nullptr)
      this->pm_5_0_sensor_->publish_state(this->pm_values_[5]);
    if (this->pm_10_0_sensor_ != nullptr)
      this->pm_10_0_sensor_->publish_state(this->pm_values_[6]);
  }

  // Publish particle count values
  if (pc_success) {
    if (this->pmc_0_1_sensor_ != nullptr)
      this->pmc_0_1_sensor_->publish_state(this->pc_values_[0]);
    if (this->pmc_0_3_sensor_ != nullptr)
      this->pmc_0_3_sensor_->publish_state(this->pc_values_[1]);
    if (this->pmc_0_5_sensor_ != nullptr)
      this->pmc_0_5_sensor_->publish_state(this->pc_values_[2]);
    if (this->pmc_1_0_sensor_ != nullptr)
      this->pmc_1_0_sensor_->publish_state(this->pc_values_[3]);
    if (this->pmc_2_5_sensor_ != nullptr)
      this->pmc_2_5_sensor_->publish_state(this->pc_values_[4]);
    if (this->pmc_5_0_sensor_ != nullptr)
      this->pmc_5_0_sensor_->publish_state(this->pc_values_[5]);
    if (this->pmc_10_0_sensor_ != nullptr)
      this->pmc_10_0_sensor_->publish_state(this->pc_values_[6]);
  }
}

bool IPS7100Component::read_pm_data_() {
  uint8_t buffer[PM_DATA_SIZE];

  // Send read command and receive data
  if (this->write(&IPS7100_CMD_READ_PM, 1) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Failed to send PM read command");
    return false;
  }

  // Small delay for sensor to prepare data
  delay(10);

  if (this->read(buffer, PM_DATA_SIZE) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Failed to read PM data");
    return false;
  }

  // Validate CRC16 checksum (last 2 bytes)
  uint16_t received_crc = (buffer[PM_DATA_SIZE - 2] << 8) | buffer[PM_DATA_SIZE - 1];
  uint16_t calculated_crc = this->calc_crc16_(buffer, PM_DATA_SIZE - 2);

  if (received_crc != calculated_crc) {
    ESP_LOGW(TAG, "PM data CRC mismatch: received 0x%04X, calculated 0x%04X", received_crc, calculated_crc);
    return false;
  }

  // Parse PM values (7 x 4-byte floats, big-endian)
  for (int i = 0; i < 7; i++) {
    union {
      uint8_t bytes[4];
      float value;
    } converter;

    // Big-endian to little-endian conversion
    converter.bytes[3] = buffer[i * 4];
    converter.bytes[2] = buffer[i * 4 + 1];
    converter.bytes[1] = buffer[i * 4 + 2];
    converter.bytes[0] = buffer[i * 4 + 3];

    this->pm_values_[i] = converter.value;
  }

  ESP_LOGV(TAG, "PM values: %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f", this->pm_values_[0], this->pm_values_[1],
           this->pm_values_[2], this->pm_values_[3], this->pm_values_[4], this->pm_values_[5], this->pm_values_[6]);

  return true;
}

bool IPS7100Component::read_pc_data_() {
  uint8_t buffer[PC_DATA_SIZE];

  // Send read command and receive data
  if (this->write(&IPS7100_CMD_READ_PC, 1) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Failed to send PC read command");
    return false;
  }

  // Small delay for sensor to prepare data
  delay(10);

  if (this->read(buffer, PC_DATA_SIZE) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Failed to read PC data");
    return false;
  }

  // Validate CRC16 checksum (last 2 bytes)
  uint16_t received_crc = (buffer[PC_DATA_SIZE - 2] << 8) | buffer[PC_DATA_SIZE - 1];
  uint16_t calculated_crc = this->calc_crc16_(buffer, PC_DATA_SIZE - 2);

  if (received_crc != calculated_crc) {
    ESP_LOGW(TAG, "PC data CRC mismatch: received 0x%04X, calculated 0x%04X", received_crc, calculated_crc);
    return false;
  }

  // Parse PC values (7 x 4-byte unsigned longs, big-endian)
  for (int i = 0; i < 7; i++) {
    this->pc_values_[i] = (static_cast<uint32_t>(buffer[i * 4]) << 24) |
                          (static_cast<uint32_t>(buffer[i * 4 + 1]) << 16) |
                          (static_cast<uint32_t>(buffer[i * 4 + 2]) << 8) | static_cast<uint32_t>(buffer[i * 4 + 3]);
  }

  ESP_LOGV(TAG, "PC values: %u, %u, %u, %u, %u, %u, %u", this->pc_values_[0], this->pc_values_[1], this->pc_values_[2],
           this->pc_values_[3], this->pc_values_[4], this->pc_values_[5], this->pc_values_[6]);

  return true;
}

uint16_t IPS7100Component::calc_crc16_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ CRC16_POLYNOMIAL;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

}  // namespace ips7100
}  // namespace esphome
