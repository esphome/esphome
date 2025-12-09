#include "DpsClass.h"
#include "xensiv_dps3xx_base.h"

namespace esphome {
namespace xensiv_dps3xx_base {

using namespace dps;

const int32_t DpsClass::scaling_facts[DPS__NUM_OF_SCAL_FACTS] = {524288, 1572864, 3670016, 7864320,
                                                                 253952, 516096,  1040384, 2088960};

////////  Constructor, Destructor, begin, end  ////////

DpsClass::DpsClass(XensivDPS3xx *dps_device) {
  // assume that initialization has failed before it has been done
  m_initFail = 1U;

  this->dps_device_ = dps_device;
}

DpsClass::~DpsClass(void) { end(); }

void DpsClass::begin() {
  // Initialize the sensor (read product ID, calibration coefficients, etc.)
  m_initFail = 0U;
  //   delay(50); // Wait 50ms
  init();
}

void DpsClass::end(void) { standby(); }

////////  Declaration of other public functions starts here  ////////

uint8_t DpsClass::getProductId(void) { return m_productID; }

uint8_t DpsClass::getRevisionId(void) { return m_revisionID; }

int16_t DpsClass::getContResults(float *tempBuffer, uint8_t &tempCount, float *prsBuffer, uint8_t &prsCount,
                                 RegMask_t fifo_empty_reg) {
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  // abort if device is not in background mode
  if (!(m_opMode & 0x04)) {
    return DPS__FAIL_TOOBUSY;
  }

  if (!tempBuffer || !prsBuffer) {
    return DPS__FAIL_UNKNOWN;
  }
  tempCount = 0U;
  prsCount = 0U;

  // while FIFO is not empty
  while (readByteBitfield(fifo_empty_reg) == 0) {
    int32_t raw_result;
    float result;
    // read next result from FIFO
    int16_t type = getFIFOvalue(&raw_result);
    switch (type) {
      case 0:  // temperature
        if (tempCount < DPS__FIFO_SIZE) {
          result = calcTemp(raw_result);
          tempBuffer[tempCount++] = result;
        }
        break;
      case 1:  // pressure
        if (prsCount < DPS__FIFO_SIZE) {
          result = calcPressure(raw_result);
          prsBuffer[prsCount++] = result;
        }
        break;
      case -1:  // read failed
        break;
    }
  }
  return DPS__SUCCEEDED;
}

int16_t DpsClass::getSingleResult(float &result) {
  // abort if initialization failed
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }

  // read finished bit for current opMode
  int16_t rdy;
  switch (m_opMode) {
    case CMD_TEMP:  // temperature
      rdy = readByteBitfield(config_registers[TEMP_RDY]);
      break;
    case CMD_PRS:  // pressure
      rdy = readByteBitfield(config_registers[PRS_RDY]);
      break;
    default:  // DPS3xx not in command mode
      return DPS__FAIL_TOOBUSY;
  }
  // read new measurement result
  switch (rdy) {
    case DPS__FAIL_UNKNOWN:  // could not read ready flag
      return DPS__FAIL_UNKNOWN;
    case 0:  // ready flag not set, measurement still in progress
      return DPS__FAIL_UNFINISHED;
    case 1:  // measurement ready, expected case
      Mode oldMode = m_opMode;
      m_opMode = IDLE;  // opcode was automatically reset by DPS3xx
      int32_t raw_val;
      switch (oldMode) {
        case CMD_TEMP:  // temperature
          getRawResult(&raw_val, registerBlocks[TEMP]);
          result = calcTemp(raw_val);
          return DPS__SUCCEEDED;  // TODO
        case CMD_PRS:             // pressure
          getRawResult(&raw_val, registerBlocks[PRS]);
          result = calcPressure(raw_val);
          return DPS__SUCCEEDED;  // TODO
        default:
          return DPS__FAIL_UNKNOWN;  // should already be filtered above
      }
  }
  return DPS__FAIL_UNKNOWN;
}

int16_t DpsClass::measureTempOnce(float &result) { return measureTempOnce(result, m_tempOsr); }

int16_t DpsClass::measureTempOnce(float &result, uint8_t oversamplingRate) {
  // Start measurement
  int16_t ret = startMeasureTempOnce(oversamplingRate);
  if (ret != DPS__SUCCEEDED) {
    return ret;
  }

  // wait until measurement is finished
  delay(calcBusyTime(0U, m_tempOsr) / DPS__BUSYTIME_SCALING);
  delay(DPS3xx__BUSYTIME_FAILSAFE);

  ret = getSingleResult(result);
  if (ret != DPS__SUCCEEDED) {
    standby();
  }
  return ret;
}

int16_t DpsClass::startMeasureTempOnce(void) { return startMeasureTempOnce(m_tempOsr); }

int16_t DpsClass::startMeasureTempOnce(uint8_t oversamplingRate) {
  // abort if initialization failed
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_opMode != IDLE) {
    return DPS__FAIL_TOOBUSY;
  }

  if (oversamplingRate != m_tempOsr) {
    // configuration of oversampling rate
    if (configTemp(0U, oversamplingRate) != DPS__SUCCEEDED) {
      return DPS__FAIL_UNKNOWN;
    }
  }

  // set device to temperature measuring mode
  return setOpMode(CMD_TEMP);
}

int16_t DpsClass::measurePressureOnce(float &result) { return measurePressureOnce(result, m_prsOsr); }

int16_t DpsClass::measurePressureOnce(float &result, uint8_t oversamplingRate) {
  // start the measurement
  int16_t ret = startMeasurePressureOnce(oversamplingRate);
  if (ret != DPS__SUCCEEDED) {
    return ret;
  }

  // wait until measurement is finished
  delay(calcBusyTime(0U, m_prsOsr) / DPS__BUSYTIME_SCALING);
  delay(DPS3xx__BUSYTIME_FAILSAFE);

  ret = getSingleResult(result);
  if (ret != DPS__SUCCEEDED) {
    standby();
  }
  return ret;
}

int16_t DpsClass::startMeasurePressureOnce(void) { return startMeasurePressureOnce(m_prsOsr); }

int16_t DpsClass::startMeasurePressureOnce(uint8_t oversamplingRate) {
  // abort if initialization failed
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_opMode != IDLE) {
    return DPS__FAIL_TOOBUSY;
  }
  // configuration of oversampling rate, lowest measure rate to avoid conflicts
  if (oversamplingRate != m_prsOsr) {
    if (configPressure(0U, oversamplingRate)) {
      return DPS__FAIL_UNKNOWN;
    }
  }
  // set device to pressure measuring mode
  return setOpMode(CMD_PRS);
}

int16_t DpsClass::startMeasureTempCont(uint8_t measureRate, uint8_t oversamplingRate) {
  // abort if initialization failed
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_opMode != IDLE) {
    return DPS__FAIL_TOOBUSY;
  }
  // abort if speed and precision are too high
  if (calcBusyTime(measureRate, oversamplingRate) >= DPS3xx__MAX_BUSYTIME) {
    return DPS__FAIL_UNFINISHED;
  }
  // update precision and measuring rate
  if (configTemp(measureRate, oversamplingRate)) {
    return DPS__FAIL_UNKNOWN;
  }

  if (enableFIFO()) {
    return DPS__FAIL_UNKNOWN;
  }
  // Start measuring in background mode
  if (DpsClass::setOpMode(CONT_TMP)) {
    return DPS__FAIL_UNKNOWN;
  }
  return DPS__SUCCEEDED;
}

int16_t DpsClass::startMeasurePressureCont(uint8_t measureRate, uint8_t oversamplingRate) {
  // abort if initialization failed
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_opMode != IDLE) {
    return DPS__FAIL_TOOBUSY;
  }
  // abort if speed and precision are too high
  if (calcBusyTime(measureRate, oversamplingRate) >= DPS3xx__MAX_BUSYTIME) {
    return DPS__FAIL_UNFINISHED;
  }
  // update precision and measuring rate
  if (configPressure(measureRate, oversamplingRate))
    return DPS__FAIL_UNKNOWN;
  // enable result FIFO
  if (enableFIFO()) {
    return DPS__FAIL_UNKNOWN;
  }
  // Start measuring in background mode
  if (DpsClass::setOpMode(CONT_PRS)) {
    return DPS__FAIL_UNKNOWN;
  }
  return DPS__SUCCEEDED;
}

int16_t DpsClass::startMeasureBothCont(uint8_t tempMr, uint8_t tempOsr, uint8_t prsMr, uint8_t prsOsr) {
  // abort if initialization failed
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_opMode != IDLE) {
    return DPS__FAIL_TOOBUSY;
  }
  // abort if speed and precision are too high
  if (calcBusyTime(tempMr, tempOsr) + calcBusyTime(prsMr, prsOsr) >= DPS3xx__MAX_BUSYTIME) {
    return DPS__FAIL_UNFINISHED;
  }
  // update precision and measuring rate
  if (configTemp(tempMr, tempOsr)) {
    return DPS__FAIL_UNKNOWN;
  }
  // update precision and measuring rate
  if (configPressure(prsMr, prsOsr))
    return DPS__FAIL_UNKNOWN;
  // enable result FIFO
  if (enableFIFO()) {
    return DPS__FAIL_UNKNOWN;
  }
  // Start measuring in background mode
  if (setOpMode(CONT_BOTH)) {
    return DPS__FAIL_UNKNOWN;
  }
  return DPS__SUCCEEDED;
}

int16_t DpsClass::standby(void) {
  // abort if initialization failed
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  // set device to idling mode
  int16_t ret = setOpMode(IDLE);
  if (ret != DPS__SUCCEEDED) {
    return ret;
  }
  ret = disableFIFO();
  return ret;
}

int16_t DpsClass::correctTemp(void) {
  if (m_initFail) {
    return DPS__FAIL_INIT_FAILED;
  }
  writeByte(0x0E, 0xA5);
  writeByte(0x0F, 0x96);
  writeByte(0x62, 0x02);
  writeByte(0x0E, 0x00);
  writeByte(0x0F, 0x00);

  // perform a first temperature measurement (again)
  // the most recent temperature will be saved internally
  // and used for compensation when calculating pressure
  float trash;
  measureTempOnce(trash);

  return DPS__SUCCEEDED;
}

int16_t DpsClass::getIntStatusFifoFull(void) { return readByteBitfield(config_registers[INT_FLAG_FIFO]); }

int16_t DpsClass::getIntStatusTempReady(void) { return readByteBitfield(config_registers[INT_FLAG_TEMP]); }

int16_t DpsClass::getIntStatusPrsReady(void) { return readByteBitfield(config_registers[INT_FLAG_PRS]); }

////////  Declaration of private functions starts here  ////////

int16_t DpsClass::setOpMode(uint8_t opMode) {
  if (writeByteBitfield(opMode, config_registers[MSR_CTRL]) == -1) {
    return DPS__FAIL_UNKNOWN;
  }
  m_opMode = (Mode) opMode;
  return DPS__SUCCEEDED;
}

int16_t DpsClass::configTemp(uint8_t tempMr, uint8_t tempOsr) {
  tempMr &= 0x07;
  tempOsr &= 0x07;
  // two accesses to the same register; for readability
  int16_t ret = writeByteBitfield(tempMr, config_registers[TEMP_MR]);
  ret = writeByteBitfield(tempOsr, config_registers[TEMP_OSR]);

  // abort immediately on fail
  if (ret != DPS__SUCCEEDED) {
    return DPS__FAIL_UNKNOWN;
  }
  m_tempMr = tempMr;
  m_tempOsr = tempOsr;
  return DPS__SUCCEEDED;
}

int16_t DpsClass::configPressure(uint8_t prsMr, uint8_t prsOsr) {
  prsMr &= 0x07;
  prsOsr &= 0x07;
  int16_t ret = writeByteBitfield(prsMr, config_registers[PRS_MR]);
  ret = writeByteBitfield(prsOsr, config_registers[PRS_OSR]);

  // abort immediately on fail
  if (ret != DPS__SUCCEEDED) {
    return DPS__FAIL_UNKNOWN;
  }
  m_prsMr = prsMr;
  m_prsOsr = prsOsr;
  return DPS__SUCCEEDED;
}

int16_t DpsClass::enableFIFO() { return writeByteBitfield(1U, config_registers[FIFO_EN]); }

int16_t DpsClass::disableFIFO() {
  int16_t ret = flushFIFO();
  ret = writeByteBitfield(0U, config_registers[FIFO_EN]);
  return ret;
}

uint16_t DpsClass::calcBusyTime(uint16_t mr, uint16_t osr) {
  // formula from datasheet (optimized)
  return ((uint32_t) 20U << mr) + ((uint32_t) 16U << (osr + mr));
}

int16_t DpsClass::getFIFOvalue(int32_t *value) {
  uint8_t buffer[DPS__RESULT_BLOCK_LENGTH] = {0};

  // abort on invalid argument or failed block reading
  if (value == NULL || readBlock(registerBlocks[PRS], buffer) != DPS__RESULT_BLOCK_LENGTH)
    return DPS__FAIL_UNKNOWN;
  *value = (uint32_t) buffer[0] << 16 | (uint32_t) buffer[1] << 8 | (uint32_t) buffer[2];
  getTwosComplement(value, 24);
  return buffer[2] & 0x01;
}

int16_t DpsClass::readByte(uint8_t regAddress) {
  uint8_t data;
  if (!this->dps_device_->read_byte(regAddress, &data)) {
    return DPS__FAIL_UNKNOWN;
  }
  return data;
}

int16_t DpsClass::writeByte(uint8_t regAddress, uint8_t data) { return writeByte(regAddress, data, 0U); }

int16_t DpsClass::writeByte(uint8_t regAddress, uint8_t data, uint8_t check) {
  if (!this->dps_device_->write_byte(regAddress, data))  // write_byte returns true on success
  {
    return DPS__FAIL_UNKNOWN;
  } else {
    if (check == 0)
      return 0;                        // no checking
    if (readByte(regAddress) == data)  // check if desired by calling function
    {
      return DPS__SUCCEEDED;
    } else {
      return DPS__FAIL_UNKNOWN;
    }
  }
}

int16_t DpsClass::writeByteBitfield(uint8_t data, RegMask_t regMask) {
  return writeByteBitfield(data, regMask.regAddress, regMask.mask, regMask.shift, 0U);
}

int16_t DpsClass::writeByteBitfield(uint8_t data, uint8_t regAddress, uint8_t mask, uint8_t shift, uint8_t check) {
  int16_t old = readByte(regAddress);
  if (old < 0) {
    // fail while reading
    return old;
  }
  return writeByte(regAddress, ((uint8_t) old & ~mask) | ((data << shift) & mask), check);
}

int16_t DpsClass::readByteBitfield(RegMask_t regMask) {
  int16_t ret = readByte(regMask.regAddress);
  if (ret < 0) {
    return ret;
  }
  return (((uint8_t) ret) & regMask.mask) >> regMask.shift;
}

int16_t DpsClass::readBlock(RegBlock_t regBlock, uint8_t *buffer) {
  // do not read if there is no buffer
  if (buffer == nullptr) {
    return 0;  // 0 bytes read successfully
  }

  // Read multiple bytes from the sensor
  if (!this->dps_device_->read_bytes(regBlock.regAddress, buffer, regBlock.length)) {
    return DPS__FAIL_UNKNOWN;
  }

  return regBlock.length;  // return number of bytes read
}

void DpsClass::getTwosComplement(int32_t *raw, uint8_t length) {
  if (*raw & ((uint32_t) 1 << (length - 1))) {
    *raw -= (uint32_t) 1 << length;
  }
}

int16_t DpsClass::getRawResult(int32_t *raw, RegBlock_t reg) {
  uint8_t buffer[DPS__RESULT_BLOCK_LENGTH] = {0};
  if (readBlock(reg, buffer) != DPS__RESULT_BLOCK_LENGTH)
    return DPS__FAIL_UNKNOWN;

  *raw = (uint32_t) buffer[0] << 16 | (uint32_t) buffer[1] << 8 | (uint32_t) buffer[2];
  getTwosComplement(raw, 24);
  return DPS__SUCCEEDED;
}

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
