#include "Dps3xx.h"

namespace esphome {
namespace xensiv_dps3xx_base {

using namespace dps;
using namespace dps3xx;

int16_t Dps3xx::getContResults(float *tempBuffer, uint8_t &tempCount, float *prsBuffer, uint8_t &prsCount) {
  return DpsClass::getContResults(tempBuffer, tempCount, prsBuffer, prsCount, registers[FIFO_EMPTY]);
}

int16_t Dps3xx::setInterruptSources(uint8_t intr_source, uint8_t polarity) {
#ifndef DPS_DISABLESPI
  // Interrupts are not supported with 4 Wire SPI
  if (!m_SpiI2c & !m_threeWire) {
    return DPS__FAIL_UNKNOWN;
  }
#endif
  return writeByteBitfield(intr_source, registers[INT_SEL]) || writeByteBitfield(polarity, registers[INT_HL]);
}

void Dps3xx::init(void) {
  int16_t prodId = readByteBitfield(registers[PROD_ID]);
  if (prodId < 0) {
    // Connected device is not a Dps3xx
    m_initFail = 1U;
    return;
  }
  m_productID = prodId;

  int16_t revId = readByteBitfield(registers[REV_ID]);
  if (revId < 0) {
    m_initFail = 1U;
    return;
  }
  m_revisionID = revId;

  // find out which temperature sensor is calibrated with coefficients...
  int16_t sensor = readByteBitfield(registers[TEMP_SENSORREC]);
  if (sensor < 0) {
    m_initFail = 1U;
    return;
  }

  //...and use this sensor for temperature measurement
  m_tempSensor = sensor;
  if (writeByteBitfield((uint8_t) sensor, registers[TEMP_SENSOR]) < 0) {
    m_initFail = 1U;
    return;
  }

  // read coefficients
  if (readcoeffs() < 0) {
    m_initFail = 1U;
    return;
  }

  // set to standby for further configuration
  standby();

  // set measurement precision and rate to standard values;
  configTemp(DPS__MEASUREMENT_RATE_4, DPS__OVERSAMPLING_RATE_8);
  configPressure(DPS__MEASUREMENT_RATE_4, DPS__OVERSAMPLING_RATE_8);

  // perform a first temperature measurement
  // the most recent temperature will be saved internally
  // and used for compensation when calculating pressure
  float trash;
  measureTempOnce(trash);

  // make sure the Dps3xx is in standby after initialization
  standby();

  // Fix IC with a fuse bit problem, which lead to a wrong temperature
  // Should not affect ICs without this problem
  correctTemp();
}

int16_t Dps3xx::readcoeffs(void) {
  // TODO: remove magic number
  uint8_t buffer[18];
  // read COEF registers to buffer
  int16_t ret = readBlock(coeffBlock, buffer);
  if (!ret)
    return DPS__FAIL_INIT_FAILED;

  // compose coefficients from buffer content
  m_c0Half = ((uint32_t) buffer[0] << 4) | (((uint32_t) buffer[1] >> 4) & 0x0F);
  getTwosComplement(&m_c0Half, 12);
  // c0 is only used as c0*0.5, so c0_half is calculated immediately
  m_c0Half = m_c0Half / 2U;

  // now do the same thing for all other coefficients
  m_c1 = (((uint32_t) buffer[1] & 0x0F) << 8) | (uint32_t) buffer[2];
  getTwosComplement(&m_c1, 12);
  m_c00 = ((uint32_t) buffer[3] << 12) | ((uint32_t) buffer[4] << 4) | (((uint32_t) buffer[5] >> 4) & 0x0F);
  getTwosComplement(&m_c00, 20);
  m_c10 = (((uint32_t) buffer[5] & 0x0F) << 16) | ((uint32_t) buffer[6] << 8) | (uint32_t) buffer[7];
  getTwosComplement(&m_c10, 20);

  m_c01 = ((uint32_t) buffer[8] << 8) | (uint32_t) buffer[9];
  getTwosComplement(&m_c01, 16);

  m_c11 = ((uint32_t) buffer[10] << 8) | (uint32_t) buffer[11];
  getTwosComplement(&m_c11, 16);
  m_c20 = ((uint32_t) buffer[12] << 8) | (uint32_t) buffer[13];
  getTwosComplement(&m_c20, 16);
  m_c21 = ((uint32_t) buffer[14] << 8) | (uint32_t) buffer[15];
  getTwosComplement(&m_c21, 16);
  m_c30 = ((uint32_t) buffer[16] << 8) | (uint32_t) buffer[17];
  getTwosComplement(&m_c30, 16);
  return DPS__SUCCEEDED;
}

int16_t Dps3xx::configTemp(uint8_t tempMr, uint8_t tempOsr) {
  int16_t ret = DpsClass::configTemp(tempMr, tempOsr);

  writeByteBitfield(m_tempSensor, registers[TEMP_SENSOR]);
  // set TEMP SHIFT ENABLE if oversampling rate higher than eight(2^3)
  if (tempOsr > DPS3xx__OSR_SE) {
    ret = writeByteBitfield(1U, registers[TEMP_SE]);
  } else {
    ret = writeByteBitfield(0U, registers[TEMP_SE]);
  }
  return ret;
}

int16_t Dps3xx::configPressure(uint8_t prsMr, uint8_t prsOsr) {
  int16_t ret = DpsClass::configPressure(prsMr, prsOsr);
  // set PM SHIFT ENABLE if oversampling rate higher than eight(2^3)
  if (prsOsr > DPS3xx__OSR_SE) {
    ret = writeByteBitfield(1U, registers[PRS_SE]);
  } else {
    ret = writeByteBitfield(0U, registers[PRS_SE]);
  }
  return ret;
}

float Dps3xx::calcTemp(int32_t raw) {
  float temp = raw;

  // scale temperature according to scaling table and oversampling
  temp /= scaling_facts[m_tempOsr];

  // update last measured temperature
  // it will be used for pressure compensation
  m_lastTempScal = temp;

  // Calculate compensated temperature
  temp = m_c0Half + m_c1 * temp;

  return temp;
}

float Dps3xx::calcPressure(int32_t raw) {
  float prs = raw;

  // scale pressure according to scaling table and oversampling
  prs /= scaling_facts[m_prsOsr];

  // Calculate compensated pressure
  prs = m_c00 + prs * (m_c10 + prs * (m_c20 + prs * m_c30)) + m_lastTempScal * (m_c01 + prs * (m_c11 + prs * m_c21));

  // return pressure
  return prs;
}

int16_t Dps3xx::flushFIFO() { return writeByteBitfield(1, registers[FIFO_FL]); }

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
