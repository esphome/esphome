/**
 * @file axp2101.h
 * @brief AXP2101 Power Management IC Component
 *
 * This component provides access to the AXP2101 PMIC which includes:
 * - 5 DCDC regulators (DCDC1-5)
 * - 11 LDO regulators (ALDO1-4, BLDO1-2, CPUSLDO, DLDO1-2)
 * - Battery management and charging
 * - ADC monitoring (battery voltage, current, temperature, etc.)
 */
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace axp2101 {

// Register addresses
static const uint8_t AXP2101_STATUS1 = 0x00;
static const uint8_t AXP2101_STATUS2 = 0x01;
static const uint8_t AXP2101_IC_TYPE = 0x03;
static const uint8_t AXP2101_COMMON_CONFIG = 0x10;
static const uint8_t AXP2101_BATFET_CTRL = 0x12;
static const uint8_t AXP2101_MIN_SYS_VOL_CTRL = 0x14;
static const uint8_t AXP2101_INPUT_VOL_LIMIT_CTRL = 0x15;
static const uint8_t AXP2101_INPUT_CUR_LIMIT_CTRL = 0x16;
static const uint8_t AXP2101_PWRON_STATUS = 0x20;
static const uint8_t AXP2101_PWROFF_STATUS = 0x21;
static const uint8_t AXP2101_PWROFF_EN = 0x22;

// ADC control and data registers
static const uint8_t AXP2101_ADC_CHANNEL_CTRL = 0x30;
static const uint8_t AXP2101_ADC_DATA_RELUST0 = 0x34;  // Battery voltage high
static const uint8_t AXP2101_ADC_DATA_RELUST1 = 0x35;  // Battery voltage low
static const uint8_t AXP2101_ADC_DATA_RELUST2 = 0x36;  // TS voltage high
static const uint8_t AXP2101_ADC_DATA_RELUST3 = 0x37;  // TS voltage low
static const uint8_t AXP2101_ADC_DATA_RELUST4 = 0x38;  // VBUS voltage high
static const uint8_t AXP2101_ADC_DATA_RELUST5 = 0x39;  // VBUS voltage low
static const uint8_t AXP2101_ADC_DATA_RELUST6 = 0x3A;  // VSYS voltage high
static const uint8_t AXP2101_ADC_DATA_RELUST7 = 0x3B;  // VSYS voltage low
static const uint8_t AXP2101_ADC_DATA_RELUST8 = 0x3C;  // Die temperature high
static const uint8_t AXP2101_ADC_DATA_RELUST9 = 0x3D;  // Die temperature low

// Interrupt registers
static const uint8_t AXP2101_INTEN1 = 0x40;
static const uint8_t AXP2101_INTEN2 = 0x41;
static const uint8_t AXP2101_INTEN3 = 0x42;
static const uint8_t AXP2101_INTSTS1 = 0x48;
static const uint8_t AXP2101_INTSTS2 = 0x49;
static const uint8_t AXP2101_INTSTS3 = 0x4A;

// Charging configuration registers
static const uint8_t AXP2101_CHARGE_GAUGE_WDT_CTRL = 0x18;
static const uint8_t AXP2101_IPRECHG_SET = 0x61;
static const uint8_t AXP2101_ICC_CHG_SET = 0x62;
static const uint8_t AXP2101_ITERM_CHG_SET_CTRL = 0x63;
static const uint8_t AXP2101_CV_CHG_VOL_SET = 0x64;
static const uint8_t AXP2101_CHARGE_TIMEOUT_SET_CTRL = 0x67;
static const uint8_t AXP2101_BAT_DET_CTRL = 0x68;
static const uint8_t AXP2101_CHGLED_SET_CTRL = 0x69;
static const uint8_t AXP2101_BTN_BAT_CHG_VOL_SET = 0x6A;

// DCDC control registers
static const uint8_t AXP2101_DC_ONOFF_DVM_CTRL = 0x80;
static const uint8_t AXP2101_DC_VOL0_CTRL = 0x82;  // DCDC1
static const uint8_t AXP2101_DC_VOL1_CTRL = 0x83;  // DCDC2
static const uint8_t AXP2101_DC_VOL2_CTRL = 0x84;  // DCDC3
static const uint8_t AXP2101_DC_VOL3_CTRL = 0x85;  // DCDC4
static const uint8_t AXP2101_DC_VOL4_CTRL = 0x86;  // DCDC5

// LDO control registers
static const uint8_t AXP2101_LDO_ONOFF_CTRL0 = 0x90;
static const uint8_t AXP2101_LDO_ONOFF_CTRL1 = 0x91;
static const uint8_t AXP2101_ALDO1_VOL_CTRL = 0x92;
static const uint8_t AXP2101_ALDO2_VOL_CTRL = 0x93;
static const uint8_t AXP2101_ALDO3_VOL_CTRL = 0x94;
static const uint8_t AXP2101_ALDO4_VOL_CTRL = 0x95;
static const uint8_t AXP2101_BLDO1_VOL_CTRL = 0x96;
static const uint8_t AXP2101_BLDO2_VOL_CTRL = 0x97;
static const uint8_t AXP2101_CPUSLDO_VOL_CTRL = 0x98;
static const uint8_t AXP2101_DLDO1_VOL_CTRL = 0x99;
static const uint8_t AXP2101_DLDO2_VOL_CTRL = 0x9A;

// Battery gauge registers
static const uint8_t AXP2101_BAT_PARAMS = 0xA1;
static const uint8_t AXP2101_BAT_GAUGE_CTRL = 0xA2;
static const uint8_t AXP2101_BAT_PERCENT_DATA = 0xA4;

// Power rail identifiers
enum PowerRail {
  DCDC1 = 0,
  DCDC2,
  DCDC3,
  DCDC4,
  DCDC5,
  ALDO1,
  ALDO2,
  ALDO3,
  ALDO4,
  BLDO1,
  BLDO2,
  CPUSLDO,
  DLDO1,
  DLDO2,
};

/**
 * @brief Main AXP2101 component class
 *
 * This component handles communication with the AXP2101 PMIC via I2C.
 * It provides methods for power rail control, voltage adjustment,
 * and monitoring of various parameters.
 */
class AXP2101Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

#ifdef USE_SENSOR
  // Sensor setters
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { this->battery_voltage_sensor_ = sensor; }
  void set_battery_current_sensor(sensor::Sensor *sensor) { this->battery_current_sensor_ = sensor; }
  void set_battery_level_sensor(sensor::Sensor *sensor) { this->battery_level_sensor_ = sensor; }
  void set_vbus_voltage_sensor(sensor::Sensor *sensor) { this->vbus_voltage_sensor_ = sensor; }
  void set_vsys_voltage_sensor(sensor::Sensor *sensor) { this->vsys_voltage_sensor_ = sensor; }
  void set_die_temperature_sensor(sensor::Sensor *sensor) { this->die_temperature_sensor_ = sensor; }
#endif

  // Power rail control methods
  bool enable_power_rail(PowerRail rail);
  bool disable_power_rail(PowerRail rail);
  bool is_power_rail_enabled(PowerRail rail);

  // Voltage control methods
  bool set_rail_voltage(PowerRail rail, uint16_t millivolts);
  uint16_t get_rail_voltage(PowerRail rail);

  // ADC reading methods
  uint16_t read_battery_voltage();
  uint16_t read_vbus_voltage();
  uint16_t read_vsys_voltage();
  float read_die_temperature();
  uint8_t read_battery_level();

 protected:
  // Helper methods for register access
  bool set_register_bit(uint8_t reg, uint8_t bit);
  bool clear_register_bit(uint8_t reg, uint8_t bit);
  bool get_register_bit(uint8_t reg, uint8_t bit);
  uint16_t read_register_h5l8(uint8_t high_reg, uint8_t low_reg);
  uint16_t read_register_h6l8(uint8_t high_reg, uint8_t low_reg);

  // Power rail helper methods
  bool get_rail_enable_info(PowerRail rail, uint8_t &reg, uint8_t &bit);
  bool get_rail_voltage_info(PowerRail rail, uint8_t &reg, uint16_t &min_mv, uint16_t &max_mv, uint16_t &step_mv);
  uint8_t calculate_voltage_register_value(PowerRail rail, uint16_t millivolts);
  uint16_t calculate_millivolts_from_register(PowerRail rail, uint8_t reg_value);

#ifdef USE_SENSOR
  // Sensor pointers (optional)
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *battery_current_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};
  sensor::Sensor *vbus_voltage_sensor_{nullptr};
  sensor::Sensor *vsys_voltage_sensor_{nullptr};
  sensor::Sensor *die_temperature_sensor_{nullptr};
#endif
};

}  // namespace axp2101
}  // namespace esphome
