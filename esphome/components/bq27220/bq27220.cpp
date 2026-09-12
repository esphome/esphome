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

// Table describing each readable standard command: which register to read, which
// sensor it feeds, and how to convert the raw 16-bit word into the published
// value (raw * scale + offset). `is_signed` reinterprets the word as int16_t;
// `sentinel_ffff` publishes NAN when the gauge reports 0xFFFF (not applicable).
namespace {
struct SensorEntry {
  sensor::Sensor *BQ27220Component::*sensor;
  float scale;
  float offset;
  uint8_t reg;
  bool is_signed;
  bool sentinel_ffff;
};
}  // namespace

void BQ27220Component::update() {
  // Defined here (inside a member function) so the pointers to the protected
  // sensor members can be formed. Still compile-time constant, so it lives in flash.
  static constexpr SensorEntry SENSOR_ENTRIES[] = {
      {&BQ27220Component::voltage_sensor_, 0.001f, 0.0f, BQ27220_REG_VOLTAGE, false, false},  // mV → V
      {&BQ27220Component::current_sensor_, 0.001f, 0.0f, BQ27220_REG_CURRENT, true, false},   // signed mA → A (§2.8)
      {&BQ27220Component::battery_level_sensor_, 1.0f, 0.0f, BQ27220_REG_STATE_OF_CHARGE, false, false},  // %
      {&BQ27220Component::temperature_sensor_, 0.1f, -273.15f, BQ27220_REG_TEMPERATURE, false, false},  // 0.1K → °C
      {&BQ27220Component::remaining_capacity_sensor_, 1.0f, 0.0f, BQ27220_REG_REMAINING_CAPACITY, false, false},  // mAh
      {&BQ27220Component::full_charge_capacity_sensor_, 1.0f, 0.0f, BQ27220_REG_FULL_CHARGE_CAPACITY, false,
       false},  // mAh
      {&BQ27220Component::time_to_empty_sensor_, 1.0f, 0.0f, BQ27220_REG_TIME_TO_EMPTY, false,
       true},  // min (0xFFFF=N/A)
      // StateOfHealth() (0x2E) is a plain 0–100% word: SLUUBD4A §2.22 defines the full 16-bit
      // value as the health percentage, with no status/flags byte packed into the high byte
      // (unlike some other TI gauges), so it is published as-is with no masking.
      {&BQ27220Component::state_of_health_sensor_, 1.0f, 0.0f, BQ27220_REG_STATE_OF_HEALTH, false, false},  // 0–100%
  };

  bool success = true;

  for (size_t i = 0; i < sizeof(SENSOR_ENTRIES) / sizeof(SENSOR_ENTRIES[0]); i++) {
    const SensorEntry &entry = SENSOR_ENTRIES[i];
    sensor::Sensor *sens = this->*(entry.sensor);
    if (sens == nullptr) {
      continue;
    }

    uint16_t raw = 0;
    if (!this->read_word_(entry.reg, raw)) {
      // A failed read means the gauge is almost certainly unresponsive, so the
      // remaining reads would each just wait out a bus timeout. Publish NAN to
      // every sensor still pending and stop early.
      for (size_t j = i; j < sizeof(SENSOR_ENTRIES) / sizeof(SENSOR_ENTRIES[0]); j++) {
        if (sensor::Sensor *pending = this->*(SENSOR_ENTRIES[j].sensor)) {
          pending->publish_state(NAN);
        }
      }
      success = false;
      break;
    }

    if (entry.sentinel_ffff && raw == 0xFFFF) {
      sens->publish_state(NAN);  // e.g. TimeToEmpty when not discharging
      continue;
    }

    float value = entry.is_signed ? static_cast<int16_t>(raw) : static_cast<float>(raw);
    sens->publish_state(value * entry.scale + entry.offset);
  }

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
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Remaining Capacity", this->remaining_capacity_sensor_);
  LOG_SENSOR("  ", "Full Charge Capacity", this->full_charge_capacity_sensor_);
  LOG_SENSOR("  ", "Time to Empty", this->time_to_empty_sensor_);
  LOG_SENSOR("  ", "State of Health", this->state_of_health_sensor_);
}

}  // namespace esphome::bq27220
