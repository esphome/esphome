#include "ips7100.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace ips7100 {

static const char *const TAG = "ips7100";

// I2C commands
static const uint8_t IPS7100_CMD_READ_PC = 0x11;  // Read particle count data
static const uint8_t IPS7100_CMD_READ_PM = 0x12;  // Read PM mass data

// Data sizes (without checksum byte - sensor doesn't send it over I2C)
static const uint8_t PC_DATA_SIZE = 28;  // 7 x 4 bytes
static const uint8_t PM_DATA_SIZE = 28;  // 7 x 4 bytes

void IPS7100Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up IPS7100...");

  // Wait for sensor to be ready after power-up
  // The sensor needs time to initialize and start measuring
  delay(1000);

  // Try to read data to verify sensor is responding
  // The first read may fail if sensor is still initializing, so retry a few times
  bool success = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    if (this->read_pm_data_()) {
      success = true;
      break;
    }
    ESP_LOGD(TAG, "Read attempt %d failed, retrying...", attempt + 1);
    delay(500);
  }

  if (!success) {
    ESP_LOGE(TAG, "Failed to communicate with IPS7100 sensor after 3 attempts");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "IPS7100 sensor initialized successfully");
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

  // Small delay between reading PM and PC data
  delay(10);

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

  // Publish particle count values (convert from #/L to #/cm³)
  // Skip values of 0xFFFFFFFF as they indicate invalid/unavailable data
  if (pc_success) {
    if (this->pmc_0_1_sensor_ != nullptr && this->pc_values_[0] != 0xFFFFFFFF)
      this->pmc_0_1_sensor_->publish_state(this->pc_values_[0] / 1000.0f);
    if (this->pmc_0_3_sensor_ != nullptr && this->pc_values_[1] != 0xFFFFFFFF)
      this->pmc_0_3_sensor_->publish_state(this->pc_values_[1] / 1000.0f);
    if (this->pmc_0_5_sensor_ != nullptr && this->pc_values_[2] != 0xFFFFFFFF)
      this->pmc_0_5_sensor_->publish_state(this->pc_values_[2] / 1000.0f);
    if (this->pmc_1_0_sensor_ != nullptr && this->pc_values_[3] != 0xFFFFFFFF)
      this->pmc_1_0_sensor_->publish_state(this->pc_values_[3] / 1000.0f);
    if (this->pmc_2_5_sensor_ != nullptr && this->pc_values_[4] != 0xFFFFFFFF)
      this->pmc_2_5_sensor_->publish_state(this->pc_values_[4] / 1000.0f);
    if (this->pmc_5_0_sensor_ != nullptr && this->pc_values_[5] != 0xFFFFFFFF)
      this->pmc_5_0_sensor_->publish_state(this->pc_values_[5] / 1000.0f);
    if (this->pmc_10_0_sensor_ != nullptr && this->pc_values_[6] != 0xFFFFFFFF)
      this->pmc_10_0_sensor_->publish_state(this->pc_values_[6] / 1000.0f);
  }
}

bool IPS7100Component::read_pm_data_() {
  uint8_t buffer[PM_DATA_SIZE];

  // Send read command and receive data
  if (this->write(&IPS7100_CMD_READ_PM, 1) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Failed to send PM read command");
    return false;
  }

  if (this->read(buffer, PM_DATA_SIZE) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Failed to read PM data");
    return false;
  }

  // Parse PM values (7 x 4-byte floats, little-endian)
  for (int i = 0; i < 7; i++) {
    union {
      uint8_t bytes[4];
      float value;
    } converter;

    // Little-endian byte order
    converter.bytes[0] = buffer[i * 4];
    converter.bytes[1] = buffer[i * 4 + 1];
    converter.bytes[2] = buffer[i * 4 + 2];
    converter.bytes[3] = buffer[i * 4 + 3];

    this->pm_values_[i] = converter.value;
  }

  // Validate PM values are within reasonable range (0-1000 µg/m³)
  // Values outside this range indicate communication errors
  for (int i = 0; i < 7; i++) {
    if (!std::isfinite(this->pm_values_[i]) || this->pm_values_[i] < 0.0f || this->pm_values_[i] > 1000.0f) {
      ESP_LOGD(TAG, "Invalid PM data detected, skipping update");
      return false;
    }
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

  if (this->read(buffer, PC_DATA_SIZE) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Failed to read PC data");
    return false;
  }

  // Parse PC values (7 x 4-byte unsigned longs, little-endian)
  for (int i = 0; i < 7; i++) {
    this->pc_values_[i] = static_cast<uint32_t>(buffer[i * 4]) | (static_cast<uint32_t>(buffer[i * 4 + 1]) << 8) |
                          (static_cast<uint32_t>(buffer[i * 4 + 2]) << 16) |
                          (static_cast<uint32_t>(buffer[i * 4 + 3]) << 24);
  }

  ESP_LOGV(TAG, "PC values: %u, %u, %u, %u, %u, %u, %u", this->pc_values_[0], this->pc_values_[1], this->pc_values_[2],
           this->pc_values_[3], this->pc_values_[4], this->pc_values_[5], this->pc_values_[6]);

  return true;
}

uint8_t IPS7100Component::calc_checksum_(const uint8_t *data, size_t len) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

}  // namespace ips7100
}  // namespace esphome
