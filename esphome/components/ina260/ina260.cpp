#include "ina260.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ina260 {

static const char *const TAG = "ina260";

// | A0   | A1   | Address |
// | GND  | GND  | 0x40    |
// | GND  | V_S+ | 0x41    |
// | GND  | SDA  | 0x42    |
// | GND  | SCL  | 0x43    |
// | V_S+ | GND  | 0x44    |
// | V_S+ | V_S+ | 0x45    |
// | V_S+ | SDA  | 0x46    |
// | V_S+ | SCL  | 0x47    |
// | SDA  | GND  | 0x48    |
// | SDA  | V_S+ | 0x49    |
// | SDA  | SDA  | 0x4A    |
// | SDA  | SCL  | 0x4B    |
// | SCL  | GND  | 0x4C    |
// | SCL  | V_S+ | 0x4D    |
// | SCL  | SDA  | 0x4E    |
// | SCL  | SCL  | 0x4F    |

enum {
  INA260_REGISTER_CONFIG = 0x00,
  INA260_REGISTER_CURRENT = 0x01,
  INA260_REGISTER_BUS_VOLTAGE = 0x02,
  INA260_REGISTER_POWER = 0x03,
  INA260_REGISTER_MASK_ENABLE = 0x06,
  INA260_REGISTER_ALERT_LIMIT = 0x07,
  INA260_REGISTER_MANUFACTURE_ID = 0xFE,
  INA260_REGISTER_DEVICE_ID = 0xFF
};

void INA260Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA260...");

  // Reset device on setup
  // 0bx000000000000000 << 15 RESET Bit (1 -> trigger reset)
  if (!this->write_byte_16(INA260_REGISTER_CONFIG, 0x8000)) {
    this->error_code_ = DEVICE_RESET_FAILED;
    this->mark_failed();
    return;
  }

  delay(2);  // NOLINT

  this->read_byte_16(INA260_REGISTER_MANUFACTURE_ID, &this->manufacture_id_);
  this->read_byte_16(INA260_REGISTER_DEVICE_ID, &this->device_id_);

  if (this->manufacture_id_ != (uint16_t) 0x5449 || this->device_id_ != (uint16_t) 0x2270) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint16_t config = 0x0000;

  // Averaging Mode AVG Bit Settings[11:9] (000 -> 1 sample, 001 -> 4 sample, 111 -> 1024 samples)
  config |= 0b0000001000000000;

  // Bus Voltage Conversion Time VBUSCT Bit Settings [8:6] (100 -> 1.1ms, 111 -> 8.244 ms)
  config |= 0b0000000100000000;

  // Mode Settings [2:0] Combinations (111 -> Shunt, Bus, Continuous)
  config |= 0b0000000000000111;

  if (!this->write_byte_16(INA260_REGISTER_CONFIG, config)) {
    this->error_code_ = FAILED_TO_UPDATE_CONFIGURATION;
    this->mark_failed();
    return;
  }
}

void INA260Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA260:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  Manufacture ID: 0x%x", this->manufacture_id_);
  ESP_LOGCONFIG(TAG, "  Device ID: 0x%x", this->device_id_);

  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);

  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Connected device does not match a known INA260 sensor");
      break;
    case DEVICE_RESET_FAILED:
      ESP_LOGE(TAG, "Device reset failed - Is the device connected?");
      break;
    case FAILED_TO_UPDATE_CONFIGURATION:
      ESP_LOGE(TAG, "Failed to update device configuration");
      break;
    case NONE:
      break;
    default:
      break;
  }
}

void INA260Component::update() {
  if (this->bus_voltage_sensor_ != nullptr) {
    uint16_t raw_bus_voltage;
    if (!this->read_byte_16(INA260_REGISTER_BUS_VOLTAGE, &raw_bus_voltage)) {
      this->status_set_warning();
      return;
    }
    float bus_voltage_v = int16_t(raw_bus_voltage) * 0.00125f;
    this->bus_voltage_sensor_->publish_state(bus_voltage_v);
  }

  if (this->current_sensor_ != nullptr) {
    uint16_t raw_current;
    if (!this->read_byte_16(INA260_REGISTER_CURRENT, &raw_current)) {
      this->status_set_warning();
      return;
    }
    float current_ma = int16_t(raw_current) * 0.00125f;
    this->current_sensor_->publish_state(current_ma);
  }

  if (this->power_sensor_ != nullptr) {
    uint16_t raw_power;
    if (!this->read_byte_16(INA260_REGISTER_POWER, &raw_power)) {
      this->status_set_warning();
      return;
    }
    float power_mw = ((int16_t(raw_power) * 10.0f) / 1000.0f);
    this->power_sensor_->publish_state(power_mw);
  }

  this->status_clear_warning();
}

}  // namespace ina260
}  // namespace esphome
