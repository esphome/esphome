#pragma once
/*
 * File:          lc709203f.h
 * Addapted by:   J.G. Aguado
 * Date:          30/11/2023
 *
 * Description:
 * This component adapts (into ESPHome) the I2C Driver for the LC709203F Battery Monitor IC,
 * based on the library from Daniel deBeer (EzSBC) available at:
 * https://github.com/EzSBC/ESP32_Bat_Pro from Daniel deBeer (EzSBC)
 */

#ifndef _LC709203F_H
#define LC709203F_H

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace lc709203f {

static const uint8_t LC709203F_I2C_ADDR = 0x0B;  // LC709203F default i2c address

// Registers per datasheet Table 6

static const uint8_t LC709203F_WO_BEFORE_RSOC =
    0x04;  // Executes RSOC initialization with sampled maximum voltage when 0xAA55 is set
static const uint8_t LC709203F_RW_THERMISTORB = 0x06;  // Sets B−constant of the thermistor to be measured.
static const uint8_t LC709203F_WO_INITRSOC = 0x07;     // Executes RSOC initialization when = 0xAA55 is set.
static const uint8_t LC709203F_RW_CELLTEMPERATURE =
    0x08;  // Read or Write Cell Temperature  For ESP32-Bar-Rev2 must write temperature, no thermistor on board
static const uint8_t LC709203F_RO_CELLVOLTAGE = 0x09;  // Read cell voltage
static const uint8_t LC709203F_RW_CURRENTDIRECTION =
    0x0A;  // Selects Auto/Charge/Discharge mode = 0x0000: Auto mode, = 0x0001: Charge mode, = 0xFFFF: Discharge mode
static const uint8_t LC709203F_RW_APA = 0x0B;  // APA Register, Table 7
static const uint8_t LC709203F_RW_APTHERMISTOR =
    0x0C;  // Sets a value to adjust temperature measurement delay timing. Defaults to0x001E
static const uint8_t LC709203F_RW_RSOC = 0x0D;       // Read cell indicator to empty in 1% steps
static const uint8_t LC709203F_RO_ITE = 0x0F;        // Read cell indicator to empty in 0.1% steps
static const uint8_t LC709203F_RO_ICVERSION = 0x11;  // Contains the ID number of the IC
static const uint8_t LC709203F_RW_PROFILE =
    0x12;  // Adjusts the battery profile for nominal and fully charged voltages, see Table 8
static const uint8_t LC709203F_RW_ALARMRSOC = 0x13;  // Alarm on percent threshold
static const uint8_t LC709203F_RW_ALARMVOLT = 0x14;  // Alarm on voltage threshold
static const uint8_t LC709203F_RW_POWERMODE =
    0x15;  // Sets sleep/power mode = 0x0001: Operational mode = 0x0002: Sleep mode
static const uint8_t LC709203F_RW_STATUSBIT =
    0x16;  // Temperature method, = 0x0000: I2C mode, = 0x0001: Thermistor mode
static const uint8_t LC709203F_RO_CELLPROFILE = 0x1A;  // Displays battery profile code

// to remove warning static uint8_t crc8(uint8_t *data, int len);

/*!  Approx cell capacity Table 7 */
using lc709203_adjustment_t = enum {
  LC709203F_APA_100MAH = 0x08,
  LC709203F_APA_200MAH = 0x0B,
  LC709203F_APA_500MAH = 0x10,
  LC709203F_APA_1000MAH = 0x19,
  LC709203F_APA_2000MAH = 0x2D,
  LC709203F_APA_3000MAH = 0x36,
};

/*!  Cell profile */
using lc709203_cell_profile_t = enum {
  LC709203_NOM3p7_Charge4p2 = 1,
  LC709203_NOM3p8_Charge4p35 = 3,
  LC709203_NOM3p8_Charge4p35_Less500mAh = 6,
  LC709203_ICR18650_SAMSUNG = 5,
  LC709203_ICR18650_PANASONIC = 4
};

/*!  Cell temperature source */
using lc709203_tempmode_t = enum {
  LC709203F_TEMPERATURE_I2C = 0x0000,
  LC709203F_TEMPERATURE_THERMISTOR = 0x0001,
};

/*!  Chip power state */
using lc709203_powermode_t = enum {
  LC709203F_POWER_OPERATE = 0x0001,
  LC709203F_POWER_SLEEP = 0x0002,
};

/*!
 *    @brief  Class that stores state and functions for interacting with
 *            the LC709203F I2C LiPo monitor
 */
// class LC709203F {
class LC709203FComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  bool begin();
  void init_rsoc();

  void set_power_mode(lc709203_powermode_t t);
  void set_cell_capacity(lc709203_adjustment_t apa);
  void set_cell_profile(lc709203_cell_profile_t t);

  uint16_t get_i_cversion();
  uint16_t cell_voltage_m_v();
  uint16_t cell_remaining_percent10();  // Remaining capacity in increments of 0.1% as integer
  uint16_t cell_state_of_charge();      // In increments of 1% as integer

  uint16_t get_thermistor_beta();
  void set_thermistor_b(uint16_t beta);

  void set_temperature_mode(lc709203_tempmode_t t);
  uint16_t get_cell_temperature();
  void set_alarm_rsoc(uint8_t percent);
  void set_alarm_voltage(float voltage);

  //  added to align with esphome
  void set_cell_voltage_sensor(sensor::Sensor *cell_voltage) { cell_voltage_ = cell_voltage; }
  void set_cell_rem_percent_sensor(sensor::Sensor *cell_rem_percent) { cell_rem_percent_ = cell_rem_percent; }
  void set_icversion_sensor(sensor::Sensor *icversion) { icversion_ = icversion; }
  void set_cell_charge_sensor(sensor::Sensor *cell_charge) { cell_charge_ = cell_charge; }
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  void write16_(uint8_t reg_address, uint16_t data);
  int16_t read16_(uint8_t reg_address);

  // added for esphome
  sensor::Sensor *cell_voltage_;
  sensor::Sensor *cell_rem_percent_;
  sensor::Sensor *icversion_;
  sensor::Sensor *cell_charge_;
};

}  // namespace lc709203f
}  // namespace esphome

#endif
