/*
 * File:          lc709203f.cpp
 * Addapted by:   J.G. Aguado
 * Date:          30/11/2023
 *
 * Description:
 * This component adapts (into ESPHome) the I2C Driver for the LC709203F Battery Monitor IC,
 * based on the library from Daniel deBeer (EzSBC) available at:
 * https://github.com/EzSBC/ESP32_Bat_Pro from Daniel deBeer (EzSBC)
 */

#include "esphome/core/log.h"

#include "lc709203f.h"
#include <Wire.h>
#include "Arduino.h"

namespace esphome {
namespace lc709203f {

static const char *TAG = "lc709203f.sensor";

/*!
 *    @brief  Instantiates a new LC709203F class
 */

uint8_t i2c_address = LC709203F_I2C_ADDR;

/*!
 *    @brief  Sets up the hardware and initializes I2C
 *    @param
 *    @return True if initialization was successful, otherwise false.
 */
bool LC709203FComponent::begin() {
  ESP_LOGCONFIG(TAG, "Starting up LC709203F sensor");
  Wire.begin();
  set_power_mode(LC709203F_POWER_OPERATE);
  set_temperature_mode(LC709203F_TEMPERATURE_THERMISTOR);

  set_cell_capacity(LC709203F_APA_2000MAH);     // jbo to suit the battery I am testing with
  set_cell_profile(LC709203_NOM3p7_Charge4p2);  // jbo to suit the battery I am testing with

  return true;
}

/*   added update and setup and dump_config for esphome
 */

/*!
 *    @brief  Sets up the hardware and battery parameters
 */
void LC709203FComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting Up  LC709203F sensor");
  Wire.begin();
  set_power_mode(LC709203F_POWER_OPERATE);
  set_temperature_mode(LC709203F_TEMPERATURE_THERMISTOR);
  set_cell_capacity(LC709203F_APA_2000MAH);     // jbo to suit the batery I am testing with
  set_cell_profile(LC709203_NOM3p7_Charge4p2);  // jbo to suit the batery I am testing with
}

/*!
 *    @brief  for esphome - Collects values when polled
 */
void LC709203FComponent::update() {
  uint16_t cuv_m_v = cell_voltage_m_v();
  uint16_t rempct = cell_remaining_percent10();
  uint16_t ic_ver = get_i_cversion();
  uint16_t celchg = cell_state_of_charge();

  ESP_LOGD(TAG, "Got Battery values: cellVoltage_mV=%d cellRemainingPercent10=%d cellStateOfCharge=%d ic=0x%x", cuv_m_v,
           rempct, celchg, ic_ver);

  if (this->cell_voltage_ != nullptr) {
    uint16_t cell_voltage = cuv_m_v;
    this->cell_voltage_->publish_state(cell_voltage / 1000.0);
  }

  if (this->cell_rem_percent_ != nullptr) {
    uint16_t cell_rem_percent = rempct;
    this->cell_rem_percent_->publish_state(cell_rem_percent / 10.0);
  }

  if (this->icversion_ != nullptr) {
    uint16_t icversion = ic_ver;
    this->icversion_->publish_state(icversion);
  }

  if (this->cell_charge_ != nullptr) {
    uint16_t cell_state_of_charge = celchg;
    this->cell_charge_->publish_state(cell_state_of_charge);
  }
}

/*!
 *    @brief  for esphome - occurs at boot
 */
void LC709203FComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LC709203F:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with LC709203F failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Cell Voltage", this->cell_voltage_);
  LOG_SENSOR("  ", "Cell Rem Pct", this->cell_rem_percent_);
  LOG_SENSOR("  ", "Cell StateCharge", this->cell_charge_);
  LOG_SENSOR("  ", "IC version", this->icversion_);
}

/*!
 *    @brief  Get IC version
 *    @return 16-bit value read from LC709203F_RO_ICVERSION registers
 */
uint16_t LC709203FComponent::get_i_cversion() {
  uint16_t vers = 0;
  vers = read16_(LC709203F_RO_ICVERSION);
  return vers;
}

/*!
 *    @brief  Initialize the RSOC algorithm
 *    @return
 */
void LC709203FComponent::init_rsoc() { write16_(LC709203F_WO_INITRSOC, 0xAA55); }

/*!
 *    @brief  Get battery voltage
 *    @return Cell voltage in milliVolt
 */
uint16_t LC709203FComponent::cell_voltage_m_v() {
  uint16_t m_v = 0;
  m_v = read16_(LC709203F_RO_CELLVOLTAGE);
  return 1000 * (m_v / 1000.0);
}

/*!
 *    @brief  Get cell remaining charge in percent (0-100%)
 *    @return point value from 0 to 1000
 */
uint16_t LC709203FComponent::cell_remaining_percent10() {
  uint16_t percent = 0;
  percent = read16_(LC709203F_RO_ITE);
  return percent;
}

/*!
 *    @brief  Get battery state of charge in percent (0-100%)
 *    @return point value from 0 to 100
 */
uint16_t LC709203FComponent::cell_state_of_charge() {
  uint16_t percent = 0;
  percent = read16_(LC709203F_RW_RSOC);
  return percent;
}

/*!
 *    @brief  Get battery thermistor temperature
 *    @return value from -20 to 60 *C  // CdB Needs testing, no thermistor on ESP32_Bat_R2 board
 */
uint16_t LC709203FComponent::get_cell_temperature() {
  uint16_t temp = 0;
  temp = read16_(LC709203F_RW_CELLTEMPERATURE);
  return temp;
}

/*!
 *    @brief  Set the temperature mode (external or internal)
 *    @param t The desired mode: LC709203F_TEMPERATURE_I2C or
 * LC709203F_TEMPERATURE_THERMISTOR
 */
void LC709203FComponent::set_temperature_mode(lc709203_tempmode_t t) {
  return write16_(LC709203F_RW_STATUSBIT, (uint16_t) t);
}

/*!
 *    @brief  Set the cell capacity,
 *    @param apa The lc709203_adjustment_t enumerated approximate cell capacity
 */
void LC709203FComponent::set_cell_capacity(lc709203_adjustment_t apa) { write16_(LC709203F_RW_APA, (uint16_t) apa); }

/*!
 *    @brief  Set the alarm pin to respond to an RSOC percentage level
 *    @param percent The threshold value, set to 0 to disable alarm
 */
void LC709203FComponent::set_alarm_rsoc(uint8_t percent) { write16_(LC709203F_RW_ALARMRSOC, percent); }

/*!
 *    @brief  Set the alarm pin to respond to a battery voltage level
 *    @param voltage The threshold value, set to 0 to disable alarm
 */
void LC709203FComponent::set_alarm_voltage(float voltage) { write16_(LC709203F_RW_ALARMVOLT, voltage * 1000); }

/*!
 *    @brief  Set the power mode, LC709203F_POWER_OPERATE or
 *            LC709203F_POWER_SLEEP
 *    @param t The power mode desired
 *    @return
 */
void LC709203FComponent::set_power_mode(lc709203_powermode_t t) { write16_(LC709203F_RW_POWERMODE, (uint16_t) t); }

/*!
 *    @brief  Set cell type
 *    @param t The profile, Table 8.  Normally 1 for 3.7 nominal 4.2V Full carge cells
 *    @return
 */
void LC709203FComponent::set_cell_profile(lc709203_cell_profile_t t) { write16_(LC709203F_RW_PROFILE, (uint16_t) t); }

/*!
 *    @brief  Get the thermistor Beta value //For completeness since we have to write it.
 *    @return The uint16_t Beta value
 */
uint16_t LC709203FComponent::get_thermistor_beta() {
  uint16_t val = 0;
  val = read16_(LC709203F_RW_THERMISTORB);
  return val;
}

/*!
 *    @brief  Set the thermistor Beta value
 *    @param b The value to set it to
 *    @return
 */
void LC709203FComponent::set_thermistor_b(uint16_t beta) { write16_(LC709203F_RW_THERMISTORB, beta); }

//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
/*
    INTERNAL I2C FUNCTIONS and CRC CALCULATION
*/

/**
 * Performs a CRC8 calculation on the supplied values.
 *
 * @param data  Pointer to the data to use when calculating the CRC8.
 * @param len   The number of bytes in 'data'.
 *
 * @return The computed CRC8 value.
 */
static uint8_t crc8(uint8_t *data, int len) {
  const uint8_t polynomial(0x07);
  uint8_t crc(0x00);

  for (int j = len; j; --j) {
    crc ^= *data++;

    for (int i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
    }
  }
  return crc;
}

// writes a 16-bit word (d) to register pointer regAddress
// when selecting a register pointer to read from, data = 0
void LC709203FComponent::write16(uint8_t reg_address, uint16_t data) {
  // Setup array to hold bytes to send including CRC-8
  uint8_t crc_array[5];
  crc_array[0] = 0x16;
  crc_array[1] = reg_address;
  crc_array[2] = static_cast<uint8_t>(data & 0xFF);         // Extract low byte
  crc_array[3] = static_cast<uint8_t>((data >> 8) & 0xFF);  // Extract high byte
  // Calculate crc of preceding four bytes and place in crcArray[4]
  crc_array[4] = crc8(crc_array, 4);
  // Device address
  Wire.beginTransmission(i2c_address);
  // Register address
  Wire.write(reg_address);
  // low byte
  Wire.write(crc_array[2]);
  // high byte
  Wire.write(crc_array[3]);
  // Send crc8
  Wire.write(crc_array[4]);
  Wire.endTransmission();
}

int16_t LC709203FComponent::read16(uint8_t reg_address) {
  int16_t data = 0;
  Wire.beginTransmission(i2c_address);
  Wire.write(reg_address);
  Wire.endTransmission(false);
  Wire.requestFrom(i2c_address, (uint8_t) 2);  // jbo added per WEMOS_SHT3x_Arduino_Library issue 7
  uint8_t low_byte_data = Wire.read();
  uint8_t high_byte_data = Wire.read();
  data = word(high_byte_data, low_byte_data);
  return (data);
}

}  // namespace lc709203f
}  // namespace esphome
