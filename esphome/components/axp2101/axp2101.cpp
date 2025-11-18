/**
 * @file axp2101.cpp
 * @brief Implementation of AXP2101 Power Management IC Component
 */
#include "axp2101.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace axp2101 {

static const char *const TAG = "axp2101";

// Temperature conversion constant (from datasheet)
static const float TEMP_CONVERSION_FACTOR = 0.1f;
static const float TEMP_OFFSET = -267.15f;

void AXP2101Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AXP2101...");

  // Read chip ID to verify communication
  uint8_t chip_id;
  if (!this->read_byte(AXP2101_IC_TYPE, &chip_id)) {
    ESP_LOGE(TAG, "Failed to read chip ID");
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "Chip ID: 0x%02X", chip_id);

  // Enable ADC channels for monitoring
  // Enable battery voltage, VBUS, VSYS, and temperature measurements
  uint8_t adc_ctrl = 0xFF;  // Enable all ADC channels
  if (!this->write_byte(AXP2101_ADC_CHANNEL_CTRL, adc_ctrl)) {
    ESP_LOGW(TAG, "Failed to configure ADC channels");
  }

  ESP_LOGCONFIG(TAG, "AXP2101 setup complete");
}

void AXP2101Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AXP2101:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AXP2101 failed!");
    return;
  }

  // Log sensor configuration
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "VBUS Voltage", this->vbus_voltage_sensor_);
  LOG_SENSOR("  ", "VSYS Voltage", this->vsys_voltage_sensor_);
  LOG_SENSOR("  ", "Die Temperature", this->die_temperature_sensor_);
}

void AXP2101Component::update() { this->update_sensors(); }

bool AXP2101Component::set_register_bit(uint8_t reg, uint8_t bit) {
  uint8_t value;
  if (!this->read_byte(reg, &value)) {
    return false;
  }
  value |= (1 << bit);
  return this->write_byte(reg, value);
}

bool AXP2101Component::clear_register_bit(uint8_t reg, uint8_t bit) {
  uint8_t value;
  if (!this->read_byte(reg, &value)) {
    return false;
  }
  value &= ~(1 << bit);
  return this->write_byte(reg, value);
}

bool AXP2101Component::get_register_bit(uint8_t reg, uint8_t bit) {
  uint8_t value;
  if (!this->read_byte(reg, &value)) {
    return false;
  }
  return (value & (1 << bit)) != 0;
}

uint16_t AXP2101Component::read_register_h5l8(uint8_t high_reg, uint8_t low_reg) {
  uint8_t high, low;
  if (!this->read_byte(high_reg, &high) || !this->read_byte(low_reg, &low)) {
    return 0;
  }
  return ((high & 0x1F) << 8) | low;
}

uint16_t AXP2101Component::read_register_h6l8(uint8_t high_reg, uint8_t low_reg) {
  uint8_t high, low;
  if (!this->read_byte(high_reg, &high) || !this->read_byte(low_reg, &low)) {
    return 0;
  }
  return ((high & 0x3F) << 8) | low;
}

bool AXP2101Component::get_rail_enable_info(PowerRail rail, uint8_t &reg, uint8_t &bit) {
  // DCDC rails are controlled by DC_ONOFF_DVM_CTRL register
  if (rail >= DCDC1 && rail <= DCDC5) {
    reg = AXP2101_DC_ONOFF_DVM_CTRL;
    bit = static_cast<uint8_t>(rail);  // DCDC1=bit0, DCDC2=bit1, etc.
    return true;
  }

  // LDO rails are controlled by LDO_ONOFF_CTRL0 and LDO_ONOFF_CTRL1
  if (rail >= ALDO1 && rail <= DLDO2) {
    if (rail <= BLDO2) {
      reg = AXP2101_LDO_ONOFF_CTRL0;
      bit = static_cast<uint8_t>(rail - ALDO1);  // ALDO1=bit0, ALDO2=bit1, etc.
    } else {
      reg = AXP2101_LDO_ONOFF_CTRL1;
      bit = static_cast<uint8_t>(rail - CPUSLDO);  // CPUSLDO=bit0, DLDO1=bit1, DLDO2=bit2
    }
    return true;
  }

  return false;
}

bool AXP2101Component::get_rail_voltage_info(PowerRail rail, uint8_t &reg, uint16_t &min_mv, uint16_t &max_mv,
                                              uint16_t &step_mv) {
  switch (rail) {
    case DCDC1:
      reg = AXP2101_DC_VOL0_CTRL;
      min_mv = 1500;
      max_mv = 3400;
      step_mv = 100;
      return true;

    case DCDC2:
    case DCDC3:
    case DCDC4:
      // These have dual ranges, handle in calculate methods
      reg = AXP2101_DC_VOL1_CTRL + (rail - DCDC2);
      min_mv = 500;
      max_mv = 1540;
      step_mv = 10;
      return true;

    case DCDC5:
      reg = AXP2101_DC_VOL4_CTRL;
      min_mv = 1400;
      max_mv = 3700;
      step_mv = 100;
      return true;

    case ALDO1:
    case ALDO2:
    case ALDO3:
    case ALDO4:
      reg = AXP2101_ALDO1_VOL_CTRL + (rail - ALDO1);
      min_mv = 500;
      max_mv = 3500;
      step_mv = 100;
      return true;

    case BLDO1:
    case BLDO2:
      reg = AXP2101_BLDO1_VOL_CTRL + (rail - BLDO1);
      min_mv = 500;
      max_mv = 3500;
      step_mv = 100;
      return true;

    case CPUSLDO:
      reg = AXP2101_CPUSLDO_VOL_CTRL;
      min_mv = 500;
      max_mv = 1400;
      step_mv = 50;
      return true;

    case DLDO1:
    case DLDO2:
      reg = AXP2101_DLDO1_VOL_CTRL + (rail - DLDO1);
      min_mv = 500;
      max_mv = 3400;
      step_mv = 100;
      return true;

    default:
      return false;
  }
}

uint8_t AXP2101Component::calculate_voltage_register_value(PowerRail rail, uint16_t millivolts) {
  // Special handling for DCDC2, DCDC3, DCDC4 with dual ranges
  if (rail == DCDC2 || rail == DCDC3 || rail == DCDC4) {
    if (millivolts >= 500 && millivolts <= 1200) {
      // Range 1: 500-1200mV in 10mV steps
      return static_cast<uint8_t>((millivolts - 500) / 10);
    } else if (millivolts >= 1220 && millivolts <= 1540) {
      // Range 2: 1220-1540mV in 20mV steps
      return static_cast<uint8_t>(70 + (millivolts - 1220) / 20);
    } else if (rail == DCDC3 && millivolts >= 1600 && millivolts <= 3400) {
      // DCDC3 has an extended range
      return static_cast<uint8_t>(87 + (millivolts - 1600) / 100);
    }
    return 0;  // Invalid voltage
  }

  // Special handling for DCDC4 extended range
  if (rail == DCDC4 && millivolts >= 1600 && millivolts <= 1840) {
    return static_cast<uint8_t>(87 + (millivolts - 1600) / 20);
  }

  // Standard calculation for other rails
  uint8_t reg;
  uint16_t min_mv, max_mv, step_mv;
  if (!this->get_rail_voltage_info(rail, reg, min_mv, max_mv, step_mv)) {
    return 0;
  }

  if (millivolts < min_mv || millivolts > max_mv) {
    return 0;  // Out of range
  }

  if ((millivolts - min_mv) % step_mv != 0) {
    return 0;  // Not aligned to step size
  }

  return static_cast<uint8_t>((millivolts - min_mv) / step_mv);
}

uint16_t AXP2101Component::calculate_millivolts_from_register(PowerRail rail, uint8_t reg_value) {
  // Special handling for DCDC2, DCDC3, DCDC4 with dual ranges
  if (rail == DCDC2 || rail == DCDC3 || rail == DCDC4) {
    if (reg_value <= 70) {
      // Range 1: 500-1200mV in 10mV steps
      return 500 + (reg_value * 10);
    } else if (reg_value <= 86) {
      // Range 2: 1220-1540mV in 20mV steps
      return 1220 + ((reg_value - 70) * 20);
    } else if (rail == DCDC3 && reg_value <= 105) {
      // DCDC3 extended range: 1600-3400mV in 100mV steps
      return 1600 + ((reg_value - 87) * 100);
    } else if (rail == DCDC4 && reg_value <= 99) {
      // DCDC4 extended range: 1600-1840mV in 20mV steps
      return 1600 + ((reg_value - 87) * 20);
    }
    return 0;  // Invalid
  }

  // Standard calculation for other rails
  uint8_t reg;
  uint16_t min_mv, max_mv, step_mv;
  if (!this->get_rail_voltage_info(rail, reg, min_mv, max_mv, step_mv)) {
    return 0;
  }

  return min_mv + (reg_value * step_mv);
}

bool AXP2101Component::enable_power_rail(PowerRail rail) {
  uint8_t reg, bit;
  if (!this->get_rail_enable_info(rail, reg, bit)) {
    return false;
  }
  return this->set_register_bit(reg, bit);
}

bool AXP2101Component::disable_power_rail(PowerRail rail) {
  uint8_t reg, bit;
  if (!this->get_rail_enable_info(rail, reg, bit)) {
    return false;
  }
  return this->clear_register_bit(reg, bit);
}

bool AXP2101Component::is_power_rail_enabled(PowerRail rail) {
  uint8_t reg, bit;
  if (!this->get_rail_enable_info(rail, reg, bit)) {
    return false;
  }
  return this->get_register_bit(reg, bit);
}

bool AXP2101Component::set_rail_voltage(PowerRail rail, uint16_t millivolts) {
  uint8_t reg;
  uint16_t min_mv, max_mv, step_mv;
  if (!this->get_rail_voltage_info(rail, reg, min_mv, max_mv, step_mv)) {
    ESP_LOGE(TAG, "Invalid power rail");
    return false;
  }

  uint8_t value = this->calculate_voltage_register_value(rail, millivolts);
  if (value == 0 && millivolts != 0) {
    ESP_LOGE(TAG, "Invalid voltage %u mV for rail", millivolts);
    return false;
  }

  return this->write_byte(reg, value);
}

uint16_t AXP2101Component::get_rail_voltage(PowerRail rail) {
  uint8_t reg;
  uint16_t min_mv, max_mv, step_mv;
  if (!this->get_rail_voltage_info(rail, reg, min_mv, max_mv, step_mv)) {
    return 0;
  }

  uint8_t value;
  if (!this->read_byte(reg, &value)) {
    return 0;
  }

  return this->calculate_millivolts_from_register(rail, value & 0x7F);
}

uint16_t AXP2101Component::read_battery_voltage() {
  uint16_t raw = this->read_register_h5l8(AXP2101_ADC_DATA_RELUST0, AXP2101_ADC_DATA_RELUST1);
  return raw;  // 1 LSB = 1mV
}

uint16_t AXP2101Component::read_vbus_voltage() {
  uint16_t raw = this->read_register_h5l8(AXP2101_ADC_DATA_RELUST4, AXP2101_ADC_DATA_RELUST5);
  return raw;  // 1 LSB = 1mV
}

uint16_t AXP2101Component::read_vsys_voltage() {
  uint16_t raw = this->read_register_h6l8(AXP2101_ADC_DATA_RELUST6, AXP2101_ADC_DATA_RELUST7);
  return raw;  // 1 LSB = 1mV
}

float AXP2101Component::read_die_temperature() {
  uint16_t raw = this->read_register_h6l8(AXP2101_ADC_DATA_RELUST8, AXP2101_ADC_DATA_RELUST9);
  // Temperature conversion: T = raw * 0.1 - 267.15
  return (raw * TEMP_CONVERSION_FACTOR) + TEMP_OFFSET;
}

uint8_t AXP2101Component::read_battery_level() {
  uint8_t level;
  if (!this->read_byte(AXP2101_BAT_PERCENT_DATA, &level)) {
    return 0;
  }
  return level > 100 ? 100 : level;
}

void AXP2101Component::update_sensors() {
  if (this->battery_voltage_sensor_ != nullptr) {
    float voltage = this->read_battery_voltage() / 1000.0f;
    this->battery_voltage_sensor_->publish_state(voltage);
  }

  if (this->battery_level_sensor_ != nullptr) {
    this->battery_level_sensor_->publish_state(this->read_battery_level());
  }

  if (this->vbus_voltage_sensor_ != nullptr) {
    float voltage = this->read_vbus_voltage() / 1000.0f;
    this->vbus_voltage_sensor_->publish_state(voltage);
  }

  if (this->vsys_voltage_sensor_ != nullptr) {
    float voltage = this->read_vsys_voltage() / 1000.0f;
    this->vsys_voltage_sensor_->publish_state(voltage);
  }

  if (this->die_temperature_sensor_ != nullptr) {
    this->die_temperature_sensor_->publish_state(this->read_die_temperature());
  }
}

}  // namespace axp2101
}  // namespace esphome
