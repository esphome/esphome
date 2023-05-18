#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "LOLIN_HP303B.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace hp303b {
static const char *TAG = "hp303b";

const int32_t LOLIN_HP303B::scaling_facts[HP303B__NUM_OF_SCAL_FACTS] = {524288, 1572864, 3670016, 7864320,
                                                                        253952, 516096,  1040384, 2088960};

// general Constants
#define HP303B__PROD_ID 0U
#define HP303B__STD_SLAVE_ADDRESS 0x77U
#define HP303B__SPI_WRITE_CMD 0x00U
#define HP303B__SPI_READ_CMD 0x80U
#define HP303B__SPI_RW_MASK 0x80U
#define HP303B__SPI_MAX_FREQ 100000U

#define HP303B__LSB 0x01U

#define HP303B__TEMP_STD_MR 2U
#define HP303B__TEMP_STD_OSR 3U
#define HP303B__PRS_STD_MR 2U
#define HP303B__PRS_STD_OSR 3U
#define HP303B__OSR_SE 3U
// we use 0.1 mS units for time calculations, so 10 units are one millisecond
#define HP303B__BUSYTIME_SCALING 10U
// DPS310 has 10 milliseconds of spare time for each synchronous measurement / per second for asynchronous measurements
// this is for error prevention on friday-afternoon-products :D
// you can set it to 0 if you dare, but there is no warranty that it will still work
#define HP303B__BUSYTIME_FAILSAFE 10U
#define HP303B__MAX_BUSYTIME ((1000U - HP303B__BUSYTIME_FAILSAFE) * HP303B__BUSYTIME_SCALING)
#define HP303B__NUM_OF_SCAL_FACTS 8

#define HP303B__SUCCEEDED 0
#define HP303B__FAIL_UNKNOWN -1
#define HP303B__FAIL_INIT_FAILED -2
#define HP303B__FAIL_TOOBUSY -3
#define HP303B__FAIL_UNFINISHED -4

// Constants for register manipulation
// SPI mode (3 or 4 wire)
#define HP303B__REG_ADR_SPI3W 0x09U
#define HP303B__REG_CONTENT_SPI3W 0x01U

// product id
#define HP303B__REG_INFO_PROD_ID HP303B__REG_ADR_PROD_ID, HP303B__REG_MASK_PROD_ID, HP303B__REG_SHIFT_PROD_ID
#define HP303B__REG_ADR_PROD_ID 0x0DU
#define HP303B__REG_MASK_PROD_ID 0x0FU
#define HP303B__REG_SHIFT_PROD_ID 0U

// revision id
#define HP303B__REG_INFO_REV_ID HP303B__REG_ADR_REV_ID, HP303B__REG_MASK_REV_ID, HP303B__REG_SHIFT_REV_ID
#define HP303B__REG_ADR_REV_ID 0x0DU
#define HP303B__REG_MASK_REV_ID 0xF0U
#define HP303B__REG_SHIFT_REV_ID 4U

// operating mode
#define HP303B__REG_INFO_OPMODE HP303B__REG_ADR_OPMODE, HP303B__REG_MASK_OPMODE, HP303B__REG_SHIFT_OPMODE
#define HP303B__REG_ADR_OPMODE 0x08U
#define HP303B__REG_MASK_OPMODE 0x07U
#define HP303B__REG_SHIFT_OPMODE 0U

// temperature measure rate
#define HP303B__REG_INFO_TEMP_MR HP303B__REG_ADR_TEMP_MR, HP303B__REG_MASK_TEMP_MR, HP303B__REG_SHIFT_TEMP_MR
#define HP303B__REG_ADR_TEMP_MR 0x07U
#define HP303B__REG_MASK_TEMP_MR 0x70U
#define HP303B__REG_SHIFT_TEMP_MR 4U

// temperature oversampling rate
#define HP303B__REG_INFO_TEMP_OSR HP303B__REG_ADR_TEMP_OSR, HP303B__REG_MASK_TEMP_OSR, HP303B__REG_SHIFT_TEMP_OSR
#define HP303B__REG_ADR_TEMP_OSR 0x07U
#define HP303B__REG_MASK_TEMP_OSR 0x07U
#define HP303B__REG_SHIFT_TEMP_OSR 0U

// temperature sensor
#define HP303B__REG_INFO_TEMP_SENSOR \
  HP303B__REG_ADR_TEMP_SENSOR, HP303B__REG_MASK_TEMP_SENSOR, HP303B__REG_SHIFT_TEMP_SENSOR
#define HP303B__REG_ADR_TEMP_SENSOR 0x07U
#define HP303B__REG_MASK_TEMP_SENSOR 0x80U
#define HP303B__REG_SHIFT_TEMP_SENSOR 7U

// temperature sensor recommendation
#define HP303B__REG_INFO_TEMP_SENSORREC \
  HP303B__REG_ADR_TEMP_SENSORREC, HP303B__REG_MASK_TEMP_SENSORREC, HP303B__REG_SHIFT_TEMP_SENSORREC
#define HP303B__REG_ADR_TEMP_SENSORREC 0x28U
#define HP303B__REG_MASK_TEMP_SENSORREC 0x80U
#define HP303B__REG_SHIFT_TEMP_SENSORREC 7U

// temperature shift enable (if temp_osr>3)
#define HP303B__REG_INFO_TEMP_SE HP303B__REG_ADR_TEMP_SE, HP303B__REG_MASK_TEMP_SE, HP303B__REG_SHIFT_TEMP_SE
#define HP303B__REG_ADR_TEMP_SE 0x09U
#define HP303B__REG_MASK_TEMP_SE 0x08U
#define HP303B__REG_SHIFT_TEMP_SE 3U

// pressure measure rate
#define HP303B__REG_INFO_PRS_MR HP303B__REG_ADR_PRS_MR, HP303B__REG_MASK_PRS_MR, HP303B__REG_SHIFT_PRS_MR
#define HP303B__REG_ADR_PRS_MR 0x06U
#define HP303B__REG_MASK_PRS_MR 0x70U
#define HP303B__REG_SHIFT_PRS_MR 4U

// pressure oversampling rate
#define HP303B__REG_INFO_PRS_OSR HP303B__REG_ADR_PRS_OSR, HP303B__REG_MASK_PRS_OSR, HP303B__REG_SHIFT_PRS_OSR
#define HP303B__REG_ADR_PRS_OSR 0x06U
#define HP303B__REG_MASK_PRS_OSR 0x07U
#define HP303B__REG_SHIFT_PRS_OSR 0U

// pressure shift enable (if prs_osr>3)
#define HP303B__REG_INFO_PRS_SE HP303B__REG_ADR_PRS_SE, HP303B__REG_MASK_PRS_SE, HP303B__REG_SHIFT_PRS_SE
#define HP303B__REG_ADR_PRS_SE 0x09U
#define HP303B__REG_MASK_PRS_SE 0x04U
#define HP303B__REG_SHIFT_PRS_SE 2U

// temperature ready flag
#define HP303B__REG_INFO_TEMP_RDY HP303B__REG_ADR_TEMP_RDY, HP303B__REG_MASK_TEMP_RDY, HP303B__REG_SHIFT_TEMP_RDY
#define HP303B__REG_ADR_TEMP_RDY 0x08U
#define HP303B__REG_MASK_TEMP_RDY 0x20U
#define HP303B__REG_SHIFT_TEMP_RDY 5U

// pressure ready flag
#define HP303B__REG_INFO_PRS_RDY HP303B__REG_ADR_PRS_RDY, HP303B__REG_MASK_PRS_RDY, HP303B__REG_SHIFT_PRS_RDY
#define HP303B__REG_ADR_PRS_RDY 0x08U
#define HP303B__REG_MASK_PRS_RDY 0x10U
#define HP303B__REG_SHIFT_PRS_RDY 4U

// pressure value
#define HP303B__REG_ADR_PRS 0x00U
#define HP303B__REG_LEN_PRS 3U

// temperature value
#define HP303B__REG_ADR_TEMP 0x03U
#define HP303B__REG_LEN_TEMP 3U

// compensation coefficients
#define HP303B__REG_ADR_COEF 0x10U
#define HP303B__REG_LEN_COEF 18

// FIFO enable
#define HP303B__REG_INFO_FIFO_EN HP303B__REG_ADR_FIFO_EN, HP303B__REG_MASK_FIFO_EN, HP303B__REG_SHIFT_FIFO_EN
#define HP303B__REG_ADR_FIFO_EN 0x09U
#define HP303B__REG_MASK_FIFO_EN 0x02U
#define HP303B__REG_SHIFT_FIFO_EN 1U

// FIFO flush
#define HP303B__REG_INFO_FIFO_FL HP303B__REG_ADR_FIFO_EN, HP303B__REG_MASK_FIFO_EN, HP303B__REG_SHIFT_FIFO_EN
#define HP303B__REG_ADR_FIFO_FL 0x0CU
#define HP303B__REG_MASK_FIFO_FL 0x80U
#define HP303B__REG_SHIFT_FIFO_FL 7U

// FIFO empty
#define HP303B__REG_INFO_FIFO_EMPTY \
  HP303B__REG_ADR_FIFO_EMPTY, HP303B__REG_MASK_FIFO_EMPTY, HP303B__REG_SHIFT_FIFO_EMPTY
#define HP303B__REG_ADR_FIFO_EMPTY 0x0BU
#define HP303B__REG_MASK_FIFO_EMPTY 0x01U
#define HP303B__REG_SHIFT_FIFO_EMPTY 0U

// FIFO full
#define HP303B__REG_INFO_FIFO_FULL HP303B__REG_ADR_FIFO_FULL, HP303B__REG_MASK_FIFO_FULL, HP303B__REG_SHIFT_FIFO_FULL
#define HP303B__REG_ADR_FIFO_FULL 0x0BU
#define HP303B__REG_MASK_FIFO_FULL 0x02U
#define HP303B__REG_SHIFT_FIFO_FULL 1U

// INT HL
#define HP303B__REG_INFO_INT_HL HP303B__REG_ADR_INT_HL, HP303B__REG_MASK_INT_HL, HP303B__REG_SHIFT_INT_HL
#define HP303B__REG_ADR_INT_HL 0x09U
#define HP303B__REG_MASK_INT_HL 0x80U
#define HP303B__REG_SHIFT_INT_HL 7U

// INT FIFO enable
#define HP303B__REG_INFO_INT_EN_FIFO \
  HP303B__REG_ADR_INT_EN_FIFO, HP303B__REG_MASK_INT_EN_FIFO, HP303B__REG_SHIFT_INT_EN_FIFO
#define HP303B__REG_ADR_INT_EN_FIFO 0x09U
#define HP303B__REG_MASK_INT_EN_FIFO 0x40U
#define HP303B__REG_SHIFT_INT_EN_FIFO 6U

// INT TEMP enable
#define HP303B__REG_INFO_INT_EN_TEMP \
  HP303B__REG_ADR_INT_EN_TEMP, HP303B__REG_MASK_INT_EN_TEMP, HP303B__REG_SHIFT_INT_EN_TEMP
#define HP303B__REG_ADR_INT_EN_TEMP 0x09U
#define HP303B__REG_MASK_INT_EN_TEMP 0x20U
#define HP303B__REG_SHIFT_INT_EN_TEMP 5U

// INT PRS enable
#define HP303B__REG_INFO_INT_EN_PRS \
  HP303B__REG_ADR_INT_EN_PRS, HP303B__REG_MASK_INT_EN_PRS, HP303B__REG_SHIFT_INT_EN_PRS
#define HP303B__REG_ADR_INT_EN_PRS 0x09U
#define HP303B__REG_MASK_INT_EN_PRS 0x10U
#define HP303B__REG_SHIFT_INT_EN_PRS 4U

// INT FIFO flag
#define HP303B__REG_INFO_INT_FLAG_FIFO \
  HP303B__REG_ADR_INT_FLAG_FIFO, HP303B__REG_MASK_INT_FLAG_FIFO, HP303B__REG_SHIFT_INT_FLAG_FIFO
#define HP303B__REG_ADR_INT_FLAG_FIFO 0x0AU
#define HP303B__REG_MASK_INT_FLAG_FIFO 0x04U
#define HP303B__REG_SHIFT_INT_FLAG_FIFO 2U

// INT TMP flag
#define HP303B__REG_INFO_INT_FLAG_TEMP \
  HP303B__REG_ADR_INT_FLAG_TEMP, HP303B__REG_MASK_INT_FLAG_TEMP, HP303B__REG_SHIFT_INT_FLAG_TEMP
#define HP303B__REG_ADR_INT_FLAG_TEMP 0x0AU
#define HP303B__REG_MASK_INT_FLAG_TEMP 0x02U
#define HP303B__REG_SHIFT_INT_FLAG_TEMP 1U

// INT PRS flag
#define HP303B__REG_INFO_INT_FLAG_PRS \
  HP303B__REG_ADR_INT_FLAG_PRS, HP303B__REG_MASK_INT_FLAG_PRS, HP303B__REG_SHIFT_INT_FLAG_PRS
#define HP303B__REG_ADR_INT_FLAG_PRS 0x0AU
#define HP303B__REG_MASK_INT_FLAG_PRS 0x01U
#define HP303B__REG_SHIFT_INT_FLAG_PRS 0U
class HP303BComponent : public PollingComponent, public spi::SPIDevice {
 public:
  void set_pressure(sensor::Sensor *pressure) { pressure_ = pressure; }

  void setup() override;
  void dump_config() override {
    ESP_LOGCONFIG(TAG, "HP303B:");
    ESP_LOGCONFIG(TAG, "  Custom Sensor Active");
    LOG_SENSOR("  ", "Pressure", this->pressure_);
  }
  void update() override;

  void standby();

  int16_t measure_pressureOnce(int32_t &result, uint8_t oversampling_rate);
  int16_t measure_pressureOnce(int32_t &result) { return measurePressureOnce(result, m_prsOsr); }
  int16_t start_measure_pressureOnce() { return startMeasurePressureOnce(m_prsOsr); };
  int16_t start_measure_pressureOnce(uint8_t over_sampling_rate);
  int16_t set_op_mode(uint8_t op_mode);
  int16_t read_byte_bitfield(uint8_t reg_address, uint8_t mask, uint8_t shift);
  int16_t writeByteBitfield(uint8_t data, uint8_t regAddress, uint8_t mask, uint8_t shift, uint8_t check);
  int16_t writeByteBitfield(uint8_t data, uint8_t regAddress, uint8_t mask, uint8_t shift);
  int16_t writeByteSpi(uint8_t regAddress, uint8_t data, uint8_t check);

  int16_t writeByte(uint8_t regAddress, uint8_t data, uint8_t check);
  int16_t writeByte(uint8_t regAddress, uint8_t data);
  int16_t readBlockSPI(uint8_t regAddress, uint8_t length, uint8_t *buffer);
  int16_t readBlock(uint8_t regAddress, uint8_t length, uint8_t *buffer);
  int16_t readByteSPI(uint8_t regAddress);
  int16_t readByte(uint8_t regAddress);
  int32_t LOLIN_HP303B::calcPressure(int32_t raw);
  int32_t LOLIN_HP303B::calcTemp(int32_t raw);
  int16_t LOLIN_HP303B::getFIFOvalue(int32_t *value);
  int16_t LOLIN_HP303B::getPressure(int32_t *result);
  int16_t LOLIN_HP303B::getTemp(int32_t *result);
  uint16_t LOLIN_HP303B::calcBusyTime(uint16_t mr, uint16_t osr);
  int16_t LOLIN_HP303B::configPressure(uint8_t prsMr, uint8_t prsOsr);
  int16_t LOLIN_HP303B::configTemp(uint8_t tempMr, uint8_t tempOsr);

 protected:
  sensor::Sensor *pressure_{nullptr};

  // scaling factor table
  static const int32_t scaling_facts[HP303B__NUM_OF_SCAL_FACTS];

  // enum for operating mode
  enum Mode {
    IDLE = 0x00,
    CMD_PRS = 0x01,
    CMD_TEMP = 0x02,
    INVAL_OP_CMD_BOTH = 0x03,   // invalid
    INVAL_OP_CONT_NONE = 0x04,  // invalid
    CONT_PRS = 0x05,
    CONT_TMP = 0x06,
    CONT_BOTH = 0x07
  };
  Mode m_opMode;

  // flags
  uint8_t m_initFail;
  uint8_t m_productID;
  uint8_t m_revisionID;

  // settings
  uint8_t m_tempMr;
  uint8_t m_tempOsr;
  uint8_t m_prsMr;
  uint8_t m_prsOsr;
  uint8_t m_tempSensor;

  // compensation coefficients
  int32_t m_c0Half;
  int32_t m_c1;
  int32_t m_c00;
  int32_t m_c10;
  int32_t m_c01;
  int32_t m_c11;
  int32_t m_c20;
  int32_t m_c21;
  int32_t m_c30;
  // last measured scaled temperature
  //(necessary for pressure compensation)
  double m_lastTempScal;

  // bus specific
  uint8_t m_SpiI2c;  // 0=SPI, 1=I2C
                     // used for I2C
  uint8_t m_slaveAddress;
  // used for SPI
  SPIClass *m_spibus;
  int32_t m_chipSelect;
  uint8_t m_threeWire;
}
}  // namespace hp303b
}  // namespace esphome