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
  ESP_LOGCONFIG(TAG, "Setting up BQ27220...");
  uint16_t device_type = 0;
  // Write DEVICE_TYPE subcommand to Control register
  uint8_t ctrl_cmd[2] = {0x01, 0x00};
  if (this->write_register(BQ27220_REG_CONTROL, ctrl_cmd, 2) != i2c::ERROR_OK) {
    this->status_set_error(LOG_STR("Failed to communicate with BQ27220"));
    this->mark_failed();
    return;
  }
  if (!this->read_word_(BQ27220_REG_CONTROL, device_type)) {
    this->status_set_error(LOG_STR("Failed to read device type from BQ27220"));
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "BQ27220 device type: 0x%04X", device_type);
}

void BQ27220Component::update() {
  uint16_t raw = 0;
  bool success = true;

  if (this->voltage_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_VOLTAGE, raw)) {
      this->voltage_sensor_->publish_state(raw / 1000.0f);  // mV → V
    } else {
      this->voltage_sensor_->publish_state(NAN);
      success = false;
    }
  }

  if (this->current_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_CURRENT, raw)) {
      this->current_sensor_->publish_state(static_cast<int16_t>(raw) / 1000.0f);  // mA → A (signed)
    } else {
      this->current_sensor_->publish_state(NAN);
      success = false;
    }
  }

  if (this->battery_level_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_STATE_OF_CHARGE, raw)) {
      this->battery_level_sensor_->publish_state(raw);  // %
    } else {
      this->battery_level_sensor_->publish_state(NAN);
      success = false;
    }
  }

  if (this->temperature_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_TEMPERATURE, raw)) {
      this->temperature_sensor_->publish_state(raw / 10.0f - 273.15f);  // 0.1K → °C
    } else {
      this->temperature_sensor_->publish_state(NAN);
      success = false;
    }
  }

  if (this->remaining_capacity_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_REMAINING_CAPACITY, raw)) {
      this->remaining_capacity_sensor_->publish_state(raw);  // mAh
    } else {
      this->remaining_capacity_sensor_->publish_state(NAN);
      success = false;
    }
  }

  if (this->full_charge_capacity_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_FULL_CHARGE_CAPACITY, raw)) {
      this->full_charge_capacity_sensor_->publish_state(raw);  // mAh
    } else {
      this->full_charge_capacity_sensor_->publish_state(NAN);
      success = false;
    }
  }

  if (this->time_to_empty_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_TIME_TO_EMPTY, raw)) {
      // 0xFFFF means not discharging / N/A; publish NAN so the sensor shows Unknown
      this->time_to_empty_sensor_->publish_state(raw == 0xFFFF ? NAN : static_cast<float>(raw));
    } else {
      this->time_to_empty_sensor_->publish_state(NAN);
      success = false;
    }
  }

  if (this->state_of_health_sensor_ != nullptr) {
    if (this->read_word_(BQ27220_REG_STATE_OF_HEALTH, raw)) {
      this->state_of_health_sensor_->publish_state(raw & 0xFF);  // lower byte = %
    } else {
      this->state_of_health_sensor_->publish_state(NAN);
      success = false;
    }
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
