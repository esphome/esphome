#include "mcp9600.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp9600 {

static const char *const TAG = "mcp9600";

static const uint8_t MCP9600_REGISTER_HOT_JUNCTION = 0x00;
// static const uint8_t MCP9600_REGISTER_JUNCTION_DELTA = 0x01; // Unused, but kept for future reference
static const uint8_t MCP9600_REGISTER_COLD_JUNTION = 0x02;
// static const uint8_t MCP9600_REGISTER_RAW_DATA_ADC = 0x03; // Unused, but kept for future reference
static const uint8_t MCP9600_REGISTER_STATUS = 0x04;
static const uint8_t MCP9600_REGISTER_SENSOR_CONFIG = 0x05;
static const uint8_t MCP9600_REGISTER_CONFIG = 0x06;
static const uint8_t MCP9600_REGISTER_ALERT1_CONFIG = 0x08;
static const uint8_t MCP9600_REGISTER_ALERT2_CONFIG = 0x09;
static const uint8_t MCP9600_REGISTER_ALERT3_CONFIG = 0x0A;
static const uint8_t MCP9600_REGISTER_ALERT4_CONFIG = 0x0B;
static const uint8_t MCP9600_REGISTER_ALERT1_HYSTERESIS = 0x0C;
static const uint8_t MCP9600_REGISTER_ALERT2_HYSTERESIS = 0x0D;
static const uint8_t MCP9600_REGISTER_ALERT3_HYSTERESIS = 0x0E;
static const uint8_t MCP9600_REGISTER_ALERT4_HYSTERESIS = 0x0F;
static const uint8_t MCP9600_REGISTER_ALERT1_LIMIT = 0x10;
static const uint8_t MCP9600_REGISTER_ALERT2_LIMIT = 0x11;
static const uint8_t MCP9600_REGISTER_ALERT3_LIMIT = 0x12;
static const uint8_t MCP9600_REGISTER_ALERT4_LIMIT = 0x13;
static const uint8_t MCP9600_REGISTER_DEVICE_ID = 0x20;

void MCP9600Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP9600...");

  uint16_t dev_id = 0;
  this->read_byte_16(MCP9600_REGISTER_DEVICE_ID, &dev_id);
  this->device_id_ = (uint8_t) (dev_id >> 8);

  // Allows both MCP9600's and MCP9601's to be connected.
  if (this->device_id_ != (uint8_t) 0x40 && this->device_id_ != (uint8_t) 0x41) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  bool success = this->write_byte(MCP9600_REGISTER_STATUS, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_SENSOR_CONFIG, uint8_t(0x00 | thermocouple_type_ << 4));
  success |= this->write_byte(MCP9600_REGISTER_CONFIG, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT1_CONFIG, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT2_CONFIG, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT3_CONFIG, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT4_CONFIG, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT1_HYSTERESIS, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT2_HYSTERESIS, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT3_HYSTERESIS, 0x00);
  success |= this->write_byte(MCP9600_REGISTER_ALERT4_HYSTERESIS, 0x00);
  success |= this->write_byte_16(MCP9600_REGISTER_ALERT1_LIMIT, 0x0000);
  success |= this->write_byte_16(MCP9600_REGISTER_ALERT2_LIMIT, 0x0000);
  success |= this->write_byte_16(MCP9600_REGISTER_ALERT3_LIMIT, 0x0000);
  success |= this->write_byte_16(MCP9600_REGISTER_ALERT4_LIMIT, 0x0000);

  if (!success) {
    this->error_code_ = FAILED_TO_UPDATE_CONFIGURATION;
    this->mark_failed();
    return;
  }
}

void MCP9600Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP9600:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  Device ID: 0x%x", this->device_id_);

  LOG_SENSOR("  ", "Hot Junction Temperature", this->hot_junction_sensor_);
  LOG_SENSOR("  ", "Cold Junction Temperature", this->cold_junction_sensor_);

  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Connected device does not match a known MCP9600 or MCP901 sensor");
      break;
    case FAILED_TO_UPDATE_CONFIGURATION:
      ESP_LOGE(TAG, "Failed to update device configuration");
      break;
    case NONE:
    default:
      break;
  }
}

void MCP9600Component::update() {
  if (this->hot_junction_sensor_ != nullptr) {
    uint16_t raw_hot_junction_temperature;
    if (!this->read_byte_16(MCP9600_REGISTER_HOT_JUNCTION, &raw_hot_junction_temperature)) {
      this->status_set_warning();
      return;
    }
    float hot_junction_temperature = int16_t(raw_hot_junction_temperature) * 0.0625;
    this->hot_junction_sensor_->publish_state(hot_junction_temperature);
  }

  if (this->cold_junction_sensor_ != nullptr) {
    uint16_t raw_cold_junction_temperature;
    if (!this->read_byte_16(MCP9600_REGISTER_COLD_JUNTION, &raw_cold_junction_temperature)) {
      this->status_set_warning();
      return;
    }
    float cold_junction_temperature = int16_t(raw_cold_junction_temperature) * 0.0625;
    this->cold_junction_sensor_->publish_state(cold_junction_temperature);
  }

  this->status_clear_warning();
}

}  // namespace mcp9600
}  // namespace esphome
