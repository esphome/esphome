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

uint8_t i2c_address = LC709203F_I2C_ADDR ;

/*!
 *    @brief  Sets up the hardware and initializes I2C
 *    @param  
 *    @return True if initialization was successful, otherwise false.
 */
bool LC709203FComponent::begin( void ) 
{

  ESP_LOGCONFIG( TAG, "Starting up LC709203F sensor");
  Wire.begin();
  setPowerMode(LC709203F_POWER_OPERATE) ;
  setTemperatureMode(LC709203F_TEMPERATURE_THERMISTOR) ;

  setCellCapacity(LC709203F_APA_2000MAH) ;     // jbo to suit the battery I am testing with
  setCellProfile( LC709203_NOM3p7_Charge4p2 ); // jbo to suit the battery I am testing with
  
  return true;
}

/*   added update and setup and dump_config for esphome
 */

/*!
 *    @brief  Sets up the hardware and battery parameters 
 */
void LC709203FComponent::setup() {

  ESP_LOGCONFIG( TAG, "Setting Up  LC709203F sensor");
  Wire.begin();
  setPowerMode(LC709203F_POWER_OPERATE) ;
  setTemperatureMode(LC709203F_TEMPERATURE_THERMISTOR) ;
  setCellCapacity(LC709203F_APA_2000MAH) ;    // jbo to suit the batery I am testing with
  setCellProfile( LC709203_NOM3p7_Charge4p2 ); // jbo to suit the batery I am testing with

}


/*!
 *    @brief  for esphome - Collects values when polled
 */
void LC709203FComponent::update() {

    uint16_t cuv_mV = cellVoltage_mV();
    uint16_t rempct = cellRemainingPercent10();
    uint16_t ic_ver = getICversion();
    uint16_t celchg = cellStateOfCharge();

    ESP_LOGD(TAG, "Got Battery values: cellVoltage_mV=%d cellRemainingPercent10=%d cellStateOfCharge=%d ic=0x%x", 
              cuv_mV, 
              rempct, 
              celchg, 
              ic_ver );

    if (this->cellVoltage_ != nullptr) {
        uint16_t cellVoltage    = cuv_mV;
        this->cellVoltage_->publish_state(cellVoltage / 1000.0);
    }

    if (this->cellRemPercent_ != nullptr) {
        uint16_t cellRemPercent = rempct;
        this->cellRemPercent_->publish_state(cellRemPercent / 10.0);
    }

    if (this->icversion_ != nullptr) {
        uint16_t icversion      = ic_ver;
        this->icversion_->publish_state(icversion);
    }

    if (this->cellCharge_ != nullptr) {
        uint16_t cellStateOfCharge      = celchg;
        this->cellCharge_->publish_state(cellStateOfCharge );
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

  LOG_SENSOR("  ", "Cell Voltage", this->cellVoltage_ );
  LOG_SENSOR("  ", "Cell Rem Pct", this->cellRemPercent_);
  LOG_SENSOR("  ", "Cell StateCharge", this->cellCharge_);
  LOG_SENSOR("  ", "IC version", this->icversion_);
}



/*!
 *    @brief  Get IC version
 *    @return 16-bit value read from LC709203F_RO_ICVERSION registers
 */
uint16_t LC709203FComponent::getICversion(void) 
{
  uint16_t vers = 0;
  vers = read16(LC709203F_RO_ICVERSION);
  return vers;
}


/*!
 *    @brief  Initialize the RSOC algorithm
 *    @return
 */
void LC709203FComponent::initRSOC(void) 
{
  write16(LC709203F_WO_INITRSOC, 0xAA55);
}


/*!
 *    @brief  Get battery voltage
 *    @return Cell voltage in milliVolt
 */
uint16_t LC709203FComponent::cellVoltage_mV(void) 
{
  uint16_t mV = 0;
  mV = read16(LC709203F_RO_CELLVOLTAGE);
  return 1000 * ( mV / 1000.0 ) ;
}


/*!
 *    @brief  Get cell remaining charge in percent (0-100%)
 *    @return point value from 0 to 1000
 */
uint16_t LC709203FComponent::cellRemainingPercent10(void) 
{
  uint16_t percent = 0;
  percent = read16(LC709203F_RO_ITE );
  return percent ;
}

/*!
 *    @brief  Get battery state of charge in percent (0-100%)
 *    @return point value from 0 to 100
 */
uint16_t LC709203FComponent::cellStateOfCharge(void)
{
  uint16_t percent = 0;
  percent = read16(LC709203F_RW_RSOC );
  return percent ;
}


/*!
 *    @brief  Get battery thermistor temperature
 *    @return value from -20 to 60 *C  // CdB Needs testing, no thermistor on ESP32_Bat_R2 board
 */
uint16_t LC709203FComponent::getCellTemperature(void) 
{
  uint16_t temp = 0;
  temp = read16(LC709203F_RW_CELLTEMPERATURE );
  return temp ;
}


/*!
 *    @brief  Set the temperature mode (external or internal)
 *    @param t The desired mode: LC709203F_TEMPERATURE_I2C or
 * LC709203F_TEMPERATURE_THERMISTOR
 */
void LC709203FComponent::setTemperatureMode(lc709203_tempmode_t t) 
{
  return write16(LC709203F_RW_STATUSBIT, (uint16_t)t);
}


/*!
 *    @brief  Set the cell capacity, 
 *    @param apa The lc709203_adjustment_t enumerated approximate cell capacity
 */
void LC709203FComponent::setCellCapacity(lc709203_adjustment_t apa) 
{
  write16(LC709203F_RW_APA, (uint16_t)apa);
}


/*!
 *    @brief  Set the alarm pin to respond to an RSOC percentage level
 *    @param percent The threshold value, set to 0 to disable alarm
 */
void LC709203FComponent::setAlarmRSOC(uint8_t percent) 
{
  write16(LC709203F_RW_ALARMRSOC, percent);
}


/*!
 *    @brief  Set the alarm pin to respond to a battery voltage level
 *    @param voltage The threshold value, set to 0 to disable alarm
 */
void LC709203FComponent::setAlarmVoltage(float voltage) 
{
  write16(LC709203F_RW_ALARMVOLT, voltage * 1000);
}


/*!
 *    @brief  Set the power mode, LC709203F_POWER_OPERATE or
 *            LC709203F_POWER_SLEEP
 *    @param t The power mode desired
 *    @return 
 */
void LC709203FComponent::setPowerMode(lc709203_powermode_t t) 
{
  write16(LC709203F_RW_POWERMODE, (uint16_t)t);
}

/*!
 *    @brief  Set cell type 
 *    @param t The profile, Table 8.  Normally 1 for 3.7 nominal 4.2V Full carge cells
 *    @return
 */
void LC709203FComponent::setCellProfile(lc709203_cell_profile_t t) 
{
  write16(LC709203F_RW_PROFILE, (uint16_t)t);
}

/*!
 *    @brief  Get the thermistor Beta value //For completeness since we have to write it.
 *    @return The uint16_t Beta value
 */
uint16_t LC709203FComponent::getThermistorBeta(void) 
{
  uint16_t val = 0;
  val = read16(LC709203F_RW_THERMISTORB);
  return val;
}


/*!
 *    @brief  Set the thermistor Beta value
 *    @param b The value to set it to
 *    @return 
 */
void LC709203FComponent::setThermistorB(uint16_t beta) 
{
  write16(LC709203F_RW_THERMISTORB, beta);
}


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
static uint8_t crc8(uint8_t *data, int len) 
{
  const uint8_t POLYNOMIAL(0x07);
  uint8_t crc(0x00);

  for (int j = len; j; --j) {
    crc ^= *data++;

    for (int i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
    }
  }
  return crc;
}


// writes a 16-bit word (d) to register pointer regAddress
// when selecting a register pointer to read from, data = 0
void LC709203FComponent::write16(uint8_t regAddress, uint16_t data)
{
  // Setup array to hold bytes to send including CRC-8
  uint8_t crcArray[5];
  crcArray[0] = 0x16;
  crcArray[1] = regAddress;
  crcArray[2] = lowByte(data);
  crcArray[3] = highByte(data);
  // Calculate crc of preceding four bytes and place in crcArray[4]
  crcArray[4] = crc8( crcArray, 4 );
  // Device address
  Wire.beginTransmission(i2c_address);
  // Register address
  Wire.write(regAddress);
  // low byte
  Wire.write(crcArray[2]);
  // high byte
  Wire.write(crcArray[3]);
  // Send crc8 
  Wire.write(crcArray[4]);
  Wire.endTransmission();
}

int16_t LC709203FComponent::read16( uint8_t regAddress)
{
  int16_t data = 0;
  Wire.beginTransmission(i2c_address);
  Wire.write(regAddress);
  Wire.endTransmission(false);
  Wire.requestFrom(i2c_address, (uint8_t)  2);   // jbo added per WEMOS_SHT3x_Arduino_Library issue 7
  uint8_t lowByteData = Wire.read();
  uint8_t highByteData = Wire.read();
  data = word(highByteData, lowByteData);
  return( data );
}

}
}