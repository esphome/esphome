#include "axp2101.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace axp2101 {

static const char *const TAG = "axp2101";

void AXP2101Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AXP2101:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up AXP2101 failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  // enable required ADV channels for TDIE and VBAT
  if (!this->write_byte(0x30, 0x1F)) {
    ESP_LOGE(TAG, "Setting ADC channels failed!");
    this->status_set_warning();
    return;
  }

  // increase ICC charging current to 1000mA
  if (!this->write_byte(0x62, 0x10)) {
    ESP_LOGE(TAG, "Setting ICC failed!");
    this->status_set_warning();
    return;
  }
}

void AXP2101Component::update() {
  this->getStatus();

  if (this->temperature_sensor_ != nullptr) {
    this->getTemperature();
  }

  if (this->battery_remaining_sensor_ != nullptr) {
    this->getBatteryPercentage();
  }

  if (this->battery_voltage_sensor_ != nullptr) {
    this->getBatteryVoltage();
  }
}

void AXP2101Component::getTemperature(void) {
  uint8_t tdie_h6, tdie_l8;

  if (this->read_register(0x3C, &tdie_h6, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  if (this->read_register(0x3D, &tdie_l8, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  ESP_LOGCONFIG(TAG, "TDIE Registers: 0x%02X 0x%02X", tdie_h6, tdie_l8);

  int16_t raw_temp = ((tdie_h6 & 0x3F) << 8) | tdie_l8;
  ESP_LOGCONFIG(TAG, "Got AXP2101 raw temperature %d", raw_temp);

  float temp = (22.0 + (7274 - (float) raw_temp) / 20.0);

  this->temperature_sensor_->publish_state(temp);
  this->status_clear_warning();
}

void AXP2101Component::getBatteryPercentage(void) {
  uint8_t per;

  if (this->read_register(0xA4, &per, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  ESP_LOGCONFIG(TAG, "PERC Register: 0x%02X", per);

  this->battery_remaining_sensor_->publish_state(per);
  this->status_clear_warning();
}

void AXP2101Component::getBatteryVoltage(void) {
  uint8_t vbat_h6, vbat_l8;

  if (this->read_register(0x34, &vbat_h6, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  if (this->read_register(0x35, &vbat_l8, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  ESP_LOGCONFIG(TAG, "VBAT Registers: 0x%02X 0x%02X", vbat_h6, vbat_l8);

  int16_t raw_volt = ((vbat_h6 & 0x3F) << 8) | vbat_l8;
  ESP_LOGCONFIG(TAG, "Got AXP2101 raw VBAT %d", raw_volt);

  float temp = ((float) raw_volt) / 1000.0;

  this->battery_voltage_sensor_->publish_state(temp);
  this->status_clear_warning();
}

void AXP2101Component::getStatus(void) {
  uint8_t pmu_status_1, pmu_status_2;

  if (this->read_register(0x00, &pmu_status_1, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  if (this->read_register(0x01, &pmu_status_2, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  ESP_LOGCONFIG(TAG, "Status Registers: 0x%02X 0x%02X", pmu_status_1, pmu_status_2);

  int16_t status = ((pmu_status_1 & 0x3F) << 8) | (pmu_status_2 & 0x7F);
  ESP_LOGCONFIG(TAG, "Got AXP2101 status %d", status);

  if (this->battery_present_sensor_ != nullptr) {
    this->battery_present_sensor_->publish_state((pmu_status_1 & 0x08) == 0x08);
  }

  if (this->battery_charging_sensor_ != nullptr) {
    this->battery_charging_sensor_->publish_state((pmu_status_2 & 0x60) == 0x20);
  }

  uint8_t chg_status = pmu_status_2 & 0x07;
  std::string chg_status_str;
  switch (chg_status) {
    case 0b000:
      chg_status_str = "tri-charge";
      break;
    case 0b001:
      chg_status_str = "pre-charge";
      break;
    case 0b010:
      chg_status_str = "constant charge";
      break;
    case 0b011:
      chg_status_str = "constant voltage";
      break;
    case 0b100:
      chg_status_str = "charge done";
      break;
    case 0b101:
      chg_status_str = "not charging";
      break;
    default:
      chg_status_str = "unknown";
  }

  if (this->battery_status_sensor_ != nullptr) {
    this->battery_status_sensor_->publish_state(chg_status_str);
  }

  this->status_clear_warning();
}

void AXP2101Component::shutdown(void) {
  uint8_t reg;

  if (this->read_register(0x10, &reg, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  reg = reg | 0x01;

  ESP_LOGI(TAG, "Attempting soft PWROFF with 0x%02X", reg);

  if (!this->write_byte(0x10, reg)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Setting PWROFF failed!");
    return;
  }
}

}  // namespace axp2101
}  // namespace esphome
