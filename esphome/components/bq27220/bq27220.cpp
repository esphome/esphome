#include "bq27220.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::bq27220 {

ESPHOME_LOG_TAG(TAG, "bq27220");

// Standard command registers (16-bit, little-endian), SLUUBD4A
static constexpr uint8_t REG_CONTROL = 0x00;
static constexpr uint8_t REG_TEMPERATURE = 0x06;           // 0.1 K
static constexpr uint8_t REG_VOLTAGE = 0x08;               // mV
static constexpr uint8_t REG_CURRENT = 0x0C;               // mA, signed
static constexpr uint8_t REG_REMAINING_CAPACITY = 0x10;    // mAh
static constexpr uint8_t REG_FULL_CHARGE_CAPACITY = 0x12;  // mAh
static constexpr uint8_t REG_TIME_TO_EMPTY = 0x16;         // min, 0xFFFF when not discharging
static constexpr uint8_t REG_STATE_OF_CHARGE = 0x2C;       // %
static constexpr uint8_t REG_STATE_OF_HEALTH = 0x2E;       // %
// The DEVICE_NUMBER subcommand (0x0001, SLUUBD4A §2.2.2) answers through the MAC data buffer
static constexpr uint8_t REG_MAC_DATA = 0x40;
static constexpr uint16_t DEVICE_NUMBER = 0x0220;
static constexpr uint16_t TIME_TO_EMPTY_NOT_APPLICABLE = 0xFFFF;

bool BQ27220Component::read_word_(uint8_t reg, uint16_t &value) {
  uint8_t data[2];
  if (this->read_register(reg, data, 2) != i2c::ERROR_OK) {
    return false;
  }
  value = encode_uint16(data[1], data[0]);
  return true;
}

void BQ27220Component::setup() {
  const uint8_t device_number_cmd[2] = {0x01, 0x00};  // DEVICE_NUMBER subcommand, little-endian
  if (this->write_register(REG_CONTROL, device_number_cmd, 2) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to communicate with BQ27220"));
    return;
  }
  delay(2);  // SLUUBD4A §5.3: the subcommand result needs a moment before it can be read
  uint16_t device_number = 0;
  if (!this->read_word_(REG_MAC_DATA, device_number)) {
    this->mark_failed(LOG_STR("Failed to read device number from BQ27220"));
    return;
  }
  if (device_number != DEVICE_NUMBER) {
    ESP_LOGE(TAG, "Unexpected device number 0x%04X (expected 0x%04X)", device_number, DEVICE_NUMBER);
    this->mark_failed();
  }
}

void BQ27220Component::update() {
  bool ok = true;
  this->publish_(this->voltage_sensor_, REG_VOLTAGE, ok, [](uint16_t raw) { return raw * 0.001f; });
  this->publish_(this->current_sensor_, REG_CURRENT, ok,
                 [](uint16_t raw) { return static_cast<int16_t>(raw) * 0.001f; });
  this->publish_(this->battery_level_sensor_, REG_STATE_OF_CHARGE, ok, [](uint16_t raw) { return float(raw); });
  this->publish_(this->temperature_sensor_, REG_TEMPERATURE, ok, [](uint16_t raw) { return raw * 0.1f - 273.15f; });
  this->publish_(this->remaining_capacity_sensor_, REG_REMAINING_CAPACITY, ok, [](uint16_t raw) { return float(raw); });
  this->publish_(this->full_charge_capacity_sensor_, REG_FULL_CHARGE_CAPACITY, ok,
                 [](uint16_t raw) { return float(raw); });
  this->publish_(this->time_to_empty_sensor_, REG_TIME_TO_EMPTY, ok,
                 [](uint16_t raw) { return raw == TIME_TO_EMPTY_NOT_APPLICABLE ? NAN : float(raw); });
  this->publish_(this->state_of_health_sensor_, REG_STATE_OF_HEALTH, ok, [](uint16_t raw) { return float(raw); });

  if (ok) {
    this->status_clear_warning();
  } else {
    this->status_set_warning(LOG_STR("Failed to read one or more registers from BQ27220"));
  }
}

void BQ27220Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ27220 Battery Fuel Gauge:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Remaining Capacity", this->remaining_capacity_sensor_);
  LOG_SENSOR("  ", "Full Charge Capacity", this->full_charge_capacity_sensor_);
  LOG_SENSOR("  ", "Time To Empty", this->time_to_empty_sensor_);
  LOG_SENSOR("  ", "State Of Health", this->state_of_health_sensor_);
}

}  // namespace esphome::bq27220
