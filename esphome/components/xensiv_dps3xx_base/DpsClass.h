/**
 * Arduino library to control Dps3xx
 *
 * "Dps3xx" represents Infineon's high-sensitive pressure and temperature sensor.
 * It measures in ranges of 300 - 1200 hPa and -40 and 85 °C.
 * The sensor can be connected via I2C.
 * It is able to perform single measurements
 * or to perform continuous measurements of temperature and pressure at the same time,
 * and stores the results in a FIFO to reduce bus communication.
 *
 * Have a look at the datasheet for more information.
 */
#ifndef DPSCLASS_H_INCLUDED
#define DPSCLASS_H_INCLUDED

#pragma once

#include "dps_config.h"

namespace esphome {
namespace xensiv_dps3xx_base {

// Forward declaration to avoid circular dependency
class XensivDPS3xx;

class DpsClass {
 public:
  // constructor
  DpsClass(XensivDPS3xx *dps_device);  // TODO CHANGE TO DPS3xx base class for constructor and so on
  // destructor
  ~DpsClass(void);

  /**
   * I2C begin function with standard address
   */
  void begin();

  /**
   * End function for Dps3xx
   * Sets the sensor to idle mode
   */
  void end(void);

  /**
   * returns the Product ID of the connected Dps3xx sensor
   */
  uint8_t getProductId(void);

  /**
   * returns the Revision ID of the connected Dps3xx sensor
   */
  uint8_t getRevisionId(void);

  /**
   * Sets the Dps3xx to standby mode
   *
   * @return  status code
   */
  int16_t standby(void);

  /**
   * performs one temperature measurement
   *
   * @param &result:      reference to a float value where the result will be written
   * @return 	status code
   */
  int16_t measureTempOnce(float &result);

  /**
   * performs one temperature measurement with specified oversamplingRate
   *
   * @param &result:              reference to a float where the result will be written
   * @param oversamplingRate:     DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2,
   *                              DPS__OVERSAMPLING_RATE_4 ... DPS__OVERSAMPLING_RATE_128,
   *                              which are defined as integers 0 - 7
   *                              The number of measurements equals to 2^n, if the value written to
   *                              the register field is n. 2^n internal measurements are combined to
   *                              return a more exact measurement
   * @return   status code
   */
  int16_t measureTempOnce(float &result, uint8_t oversamplingRate);

  /**
   * starts a single temperature measurement
   *
   * @return 	status code
   */
  int16_t startMeasureTempOnce(void);

  /**
   * starts a single temperature measurement with specified oversamplingRate
   *
   * @param oversamplingRate:     DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2,
   *                              DPS__OVERSAMPLING_RATE_4 ... DPS__OVERSAMPLING_RATE_128, which are defined as integers
   * 0 - 7
   * @return  status code
   */
  int16_t startMeasureTempOnce(uint8_t oversamplingRate);

  /**
   * performs one pressure measurement
   *
   * @param &result:              reference to a float value where the result will be written
   * @return 	status code
   */
  int16_t measurePressureOnce(float &result);

  /**
   * performs one pressure measurement with specified oversamplingRate
   *
   * @param &result:              reference to a float where the result will be written
   * @param oversamplingRate:     DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2,
   *                              DPS__OVERSAMPLING_RATE_4 ... DPS__OVERSAMPLING_RATE_128
   * @return  status code
   */
  int16_t measurePressureOnce(float &result, uint8_t oversamplingRate);

  /**
   * starts a single pressure measurement
   *
   * @return 	status code
   */
  int16_t startMeasurePressureOnce(void);

  /**
   * starts a single pressure measurement with specified oversamplingRate
   *
   * @param oversamplingRate:     DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2,
   *                              DPS__OVERSAMPLING_RATE_4 ... DPS__OVERSAMPLING_RATE_128
   * @return  status code
   */
  int16_t startMeasurePressureOnce(uint8_t oversamplingRate);

  /**
   * gets the result a single temperature or pressure measurement in °C or Pa
   *
   * @param &result:              reference to a float value where the result will be written
   * @return 	status code
   */
  int16_t getSingleResult(float &result);

  /**
   * starts a continuous temperature measurement with specified measurement rate and oversampling rate
   * If measure rate is n and oversampling rate is m, the DPS3xx performs 2^(n+m) internal measurements per second.
   * The DPS3xx cannot operate with high precision and high speed at the same time. Consult the datasheet for more
   * information.
   *
   * @param measureRate:          DPS__MEASUREMENT_RATE_1, DPS__MEASUREMENT_RATE_2,DPS__MEASUREMENT_RATE_4 ...
   * DPS__MEASUREMENT_RATE_128
   * @param oversamplingRate:     DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2, DPS__OVERSAMPLING_RATE_4 ...
   * DPS__OVERSAMPLING_RATE_128
   *
   * @return  status code
   *
   */
  int16_t startMeasureTempCont(uint8_t measureRate, uint8_t oversamplingRate);

  /**
   * starts a continuous temperature measurement with specified measurement rate and oversampling rate
   *
   * @param measureRate:          DPS__MEASUREMENT_RATE_1, DPS__MEASUREMENT_RATE_2,DPS__MEASUREMENT_RATE_4 ...
   * DPS__MEASUREMENT_RATE_128
   * @param oversamplingRate:     DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2, DPS__OVERSAMPLING_RATE_4 ...
   * DPS__OVERSAMPLING_RATE_128
   * @return  status code
   */
  int16_t startMeasurePressureCont(uint8_t measureRate, uint8_t oversamplingRate);

  /**
   * starts a continuous temperature and pressure measurement with specified measurement rate and oversampling rate for
   * temperature and pressure measurement respectively.
   *
   * @param tempMr:               measure rate for temperature
   * @param tempOsr:              oversampling rate for temperature
   * @param prsMr:                measure rate for pressure
   * @param prsOsr:               oversampling rate for pressure
   * @return  status code
   */
  int16_t startMeasureBothCont(uint8_t tempMr, uint8_t tempOsr, uint8_t prsMr, uint8_t prsOsr);

  /**
   * Gets the interrupt status flag of the FIFO
   *
   * @return 	    1 if the FIFO is full and caused an interrupt
   *              0 if the FIFO is not full or FIFO interrupt is disabled
   *              -1 on fail
   */
  int16_t getIntStatusFifoFull(void);

  /**
   * Gets the interrupt status flag that indicates a finished temperature measurement
   *
   * @return 	    1 if a finished temperature measurement caused an interrupt;
   *              0 if there is no finished temperature measurement or interrupts are disabled;
   *              -1 on fail.
   */
  int16_t getIntStatusTempReady(void);

  /**
   * Gets the interrupt status flag that indicates a finished pressure measurement
   *
   * @return      1 if a finished pressure measurement caused an interrupt;
   *              0 if there is no finished pressure measurement or interrupts are disabled;
   *              -1 on fail.
   */
  int16_t getIntStatusPrsReady(void);

  /**
   * Function to fix a hardware problem on some devices
   * You have this problem if you measure a temperature which is too high (e.g. 60°C when temperature is around 20°C)
   * Call correctTemp() directly after begin() to fix this issue
   */
  int16_t correctTemp(void);

  uint8_t m_initFail;
  dps::Mode m_opMode;

 protected:
  XensivDPS3xx *dps_device_;

  // scaling factor table
  static const int32_t scaling_facts[DPS__NUM_OF_SCAL_FACTS];

  // flags

  uint8_t m_productID;
  uint8_t m_revisionID;

  // settings
  uint8_t m_tempMr;
  uint8_t m_tempOsr;
  uint8_t m_prsMr;
  uint8_t m_prsOsr;

  // compensation coefficients for both dps3xx and dps422
  int32_t m_c00;
  int32_t m_c10;
  int32_t m_c01;
  int32_t m_c11;
  int32_t m_c20;
  int32_t m_c21;
  int32_t m_c30;

  // last measured scaled temperature (necessary for pressure compensation)
  float m_lastTempScal;

  // used for I2C
  uint8_t m_slaveAddress;

  /**
   * Initializes the sensor.
   * This function has to be called from begin()
   * and requires a valid bus initialization.
   */
  virtual void init(void) = 0;

  /**
   * reads the compensation coefficients from the sensor
   * this is called once from init(), which is called from begin()
   *
   * @return 	0 on success, -1 on fail
   */
  virtual int16_t readcoeffs(void) = 0;

  /**
   * Sets the Operation Mode of the sensor
   *
   * @param opMode:           the new OpMode as defined by dps::Mode; CMD_BOTH should not be used for DPS3xx
   * @return                  0 on success,
   *                          -1 on fail
   */
  int16_t setOpMode(uint8_t opMode);

  /**
   * Configures temperature measurement
   *
   * @param temp_mr:          DPS__MEASUREMENT_RATE_1, DPS__MEASUREMENT_RATE_2,DPS__MEASUREMENT_RATE_4 ...
   * DPS__MEASUREMENT_RATE_128
   * @param temp_osr:         DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2, DPS__OVERSAMPLING_RATE_4 ...
   * DPS__OVERSAMPLING_RATE_128
   *
   * @return 	0 normally or -1 on fail
   */
  virtual int16_t configTemp(uint8_t temp_mr, uint8_t temp_osr);

  /**
   * Configures pressure measurement
   *
   * @param prs_mr:           DPS__MEASUREMENT_RATE_1, DPS__MEASUREMENT_RATE_2,DPS__MEASUREMENT_RATE_4 ...
   * DPS__MEASUREMENT_RATE_128
   * @param prs_osr:          DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2, DPS__OVERSAMPLING_RATE_4 ...
   * DPS__OVERSAMPLING_RATE_128
   * @return 	                0 normally or
   *                          -1 on fail
   */
  virtual int16_t configPressure(uint8_t prs_mr, uint8_t prs_osr);

  virtual int16_t flushFIFO() = 0;

  virtual float calcTemp(int32_t raw) = 0;

  virtual float calcPressure(int32_t raw) = 0;

  int16_t enableFIFO();

  int16_t disableFIFO();

  /**
   * calculates the time that the sensor needs for 2^mr measurements with an oversampling rate of 2^osr (see table
   * "pressure measurement time (ms) versus oversampling rate") Note that the total measurement time for temperature and
   * pressure must not be more than 1 second. Timing behavior of pressure and temperature sensors can be considered the
   * same.
   *
   * @param mr:               DPS__MEASUREMENT_RATE_1, DPS__MEASUREMENT_RATE_2,DPS__MEASUREMENT_RATE_4 ...
   * DPS__MEASUREMENT_RATE_128
   * @param osr:              DPS__OVERSAMPLING_RATE_1, DPS__OVERSAMPLING_RATE_2, DPS__OVERSAMPLING_RATE_4 ...
   * DPS__OVERSAMPLING_RATE_128
   * @return time that the sensor needs for this measurement
   */
  uint16_t calcBusyTime(uint16_t temp_rate, uint16_t temp_osr);

  /**
   * reads the next raw value from the FIFO
   *
   * @param value:  the raw pressure or temperature value read from the pressure register blocks, where the LSB of
   * PRS_B0 marks whether the value is a temperature or a pressure.
   *
   * @return          -1 on fail
   *                  0 if result is a temperature raw value
   *                  1 if result is a pressure raw value
   */
  int16_t getFIFOvalue(int32_t *value);

  /**
   * Gets the results from continuous measurements and writes them to given arrays
   *
   * @param *tempBuffer:      The start address of the buffer where the temperature results are written
   *                          If this is NULL, no temperature results will be written out
   * @param &tempCount:       The size of the buffer for temperature results.
   *                          When the function ends, it will contain the number of bytes written to the buffer.
   * @param *prsBuffer:       The start address of the buffer where the pressure results are written
   *                          If this is NULL, no pressure results will be written out
   * @param &prsCount:        The size of the buffer for pressure results.
   *                          When the function ends, it will contain the number of bytes written to the buffer.
   * @param reg               The FIFO empty register field; needed since this field is different for each sensor
   * @return  status code
   */
  int16_t getContResults(float *tempBuffer, uint8_t &tempCount, float *prsBuffer, uint8_t &prsCount, RegMask_t reg);

  /**
   * reads a byte from the sensor
   *
   * @param regAddress:        Address that has to be read
   * @return  register content or -1 on fail
   */
  int16_t readByte(uint8_t regAddress);

  /**
   * reads a block from the sensor
   *
   * @param regAddress:       Address that has to be read
   * @param length:           Length of data block
   * @param buffer:           Buffer where data will be stored
   * @return  number of bytes that have been read successfully, which might not always equal to length due to rx-Buffer
   * overflow etc.
   */
  int16_t readBlock(RegBlock_t regBlock, uint8_t *buffer);

  /**
   * writes a byte to a given register of the sensor without checking
   *
   * @param regAddress:       Address of the register that has to be updated
   * @param data:             Byte that will be written to the register
   * @return  0 if byte was written successfully or
   *          -1 on fail
   */
  int16_t writeByte(uint8_t regAddress, uint8_t data);

  /**
   * writes a byte to a register of the sensor
   *
   * @param regAddress:       Address of the register that has to be updated
   * @param data:             Byte that will be written to the register
   * @param check:            If this is true, register content will be read after writing
   *                          to check if update was successful
   * @return  0 if byte was written successfully or
   *          -1 on fail
   */
  int16_t writeByte(uint8_t regAddress, uint8_t data, uint8_t check);

  /**
   * updates a bit field of the sensor without checking
   *
   * @param regMask:          Mask of the register that has to be updated
   * @param data:             BitValues that will be written to the register
   * @return  0 if byte was written successfully or
   *          -1 on fail
   */
  int16_t writeByteBitfield(uint8_t data, RegMask_t regMask);

  /**
   * updates a bit field of the sensor
   *
   * regMask:             Mask of the register that has to be updated
   * data:                BitValues that will be written to the register
   * check:               enables/disables check after writing; 0 disables check.
   *                      if check fails, -1 will be returned
   * @return  0 if byte was written successfully or
   *          -1 on fail
   */
  int16_t writeByteBitfield(uint8_t data, uint8_t regAddress, uint8_t mask, uint8_t shift, uint8_t check);

  /**
   * reads a bit field from the sensor
   * regMask:             Mask of the register that has to be updated
   * data:                BitValues that will be written to the register
   * @return  read and processed bits or -1 on fail
   */
  int16_t readByteBitfield(RegMask_t regMask);

  /**
   * @brief converts non-32-bit negative numbers to 32-bit negative numbers with 2's complement
   *
   * @param raw The raw number of less than 32 bits
   * @param length The bit length
   */
  void getTwosComplement(int32_t *raw, uint8_t length);

  /**
   * @brief Get a raw result from a given register block
   *
   * @param raw The address where the raw value is to be written
   * @param reg The register block to be read from
   * @return status code
   */
  int16_t getRawResult(int32_t *raw, RegBlock_t reg);
};

}  // namespace xensiv_dps3xx_base
}  // namespace esphome

#endif  // DPSCLASS_H_INCLUDED
