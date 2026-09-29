#include "bq27220.h"
#include "esphome/core/log.h"

namespace esphome::bq27220 {

static const char *const TAG = "bq27220";

bool BQ27220Component::read_word_(uint8_t reg, uint16_t &value) {
  uint8_t data[2];
  if (this->read_register(reg, data, 2) != i2c::ERROR_OK) {
    return false;
  }
  value = (uint16_t(data[1]) << 8) | data[0];
  return true;
}

void BQ27220Component::setup() {
  // Verify the device: issue the DEVICE_NUMBER subcommand via Control(), then read
  // the result from the MAC data buffer and confirm it matches the BQ27220.
  uint8_t ctrl_cmd[2] = {0x01, 0x00};  // DEVICE_NUMBER subcommand (0x0001, little-endian)
  if (this->write_register(BQ27220_REG_CONTROL, ctrl_cmd, 2) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to communicate with BQ27220"));
    return;
  }
  delay(2);  // SLUUBD4A §5.3: allow the subcommand result to update before reading
  uint16_t device_number = 0;
  if (!this->read_word_(BQ27220_REG_MAC_DATA, device_number)) {
    this->mark_failed(LOG_STR("Failed to read device number from BQ27220"));
    return;
  }
  if (device_number != BQ27220_DEVICE_NUMBER) {
    ESP_LOGE(TAG, "Unexpected device number 0x%04X (expected 0x%04X)", device_number, BQ27220_DEVICE_NUMBER);
    this->mark_failed();
    return;
  }
}

void BQ27220Component::update() {
  if (this->is_failed())
    return;

  // Read every standard command into the decoded struct, then broadcast it to the
  // per-sensor listeners registered at codegen time. Reading all registers each
  // poll (rather than only the configured ones) keeps the listeners free of I/O;
  // at the polling cadence the extra word reads are negligible.
  BQ27220Data data{};
  bool success = true;
  uint16_t raw = 0;

  if (this->read_word_(BQ27220_REG_VOLTAGE, raw)) {
    data.voltage = static_cast<float>(raw) * 0.001f;  // mV → V
  } else {
    success = false;
  }
  if (this->read_word_(BQ27220_REG_CURRENT, raw)) {
    data.current = static_cast<int16_t>(raw) * 0.001f;  // signed mA → A (§2.8)
  } else {
    success = false;
  }
  if (this->read_word_(BQ27220_REG_STATE_OF_CHARGE, raw)) {
    data.battery_level = static_cast<float>(raw);  // %
  } else {
    success = false;
  }
  if (this->read_word_(BQ27220_REG_TEMPERATURE, raw)) {
    data.temperature = static_cast<float>(raw) * 0.1f - 273.15f;  // 0.1 K → °C
  } else {
    success = false;
  }
  if (this->read_word_(BQ27220_REG_REMAINING_CAPACITY, raw)) {
    data.remaining_capacity = static_cast<float>(raw);  // mAh
  } else {
    success = false;
  }
  if (this->read_word_(BQ27220_REG_FULL_CHARGE_CAPACITY, raw)) {
    data.full_charge_capacity = static_cast<float>(raw);  // mAh
  } else {
    success = false;
  }
  if (this->read_word_(BQ27220_REG_TIME_TO_EMPTY, raw)) {
    if (raw != 0xFFFF) {                             // 0xFFFF = not discharging / not applicable
      data.time_to_empty = static_cast<float>(raw);  // min
    }
  } else {
    success = false;
  }
  // StateOfHealth() (0x2E) is a plain 0–100% word: SLUUBD4A §2.22 defines the full
  // 16-bit value as the health percentage, with no status/flags byte packed into the
  // high byte (unlike some other TI gauges), so it is used as-is with no masking.
  if (this->read_word_(BQ27220_REG_STATE_OF_HEALTH, raw)) {
    data.state_of_health = static_cast<float>(raw);  // 0–100%
  } else {
    success = false;
  }

  this->data_callback_.call(data);

  if (success) {
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
}

}  // namespace esphome::bq27220
