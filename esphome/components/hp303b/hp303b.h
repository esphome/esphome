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

INT16_MAX

// general Constants
static const int16_t HP303B__PROD_ID = 0U;

static const uint8_t HP303B__STD_SLAVE_ADDRESS = 0x77U;
static const uint8_t HP303B__SPI_WRITE_CMD = 0x00U;
static const uint8_t HP303B__SPI_READ_CMD = 0x80U;
static const uint8_t HP303B__SPI_RW_MASK = 0x80U;
static const uint8_t HP303B__SPI_MAX_FREQ = 100000U;

static const uint8_t HP303B__LSB = 0x01U;

static const uint8_t HP303B__TEMP_STD_MR = 2U;
static const uint8_t HP303B__TEMP_STD_OSR = 3U;
static const uint8_t HP303B__PRS_STD_MR = 2U;
static const uint8_t HP303B__PRS_STD_OSR = 3U;
static const uint8_t HP303B__OSR_SE = 3U;
// we use 0.1 mS units for time calculations, so 10 units are one millisecond
static const uint8_t HP303B__BUSYTIME_SCALING = 10U;
// DPS310 has 10 milliseconds of spare time for each synchronous measurement / per second for asynchronous measurements
// this is for error prevention on friday-afternoon-products :D
// can set it to 0 if you dare, but there is no warranty that it will still work
static const uint8_t HP303B__BUSYTIME_FAILSAFE = 10U;
static const uint8_t HP303B__MAX_BUSYTIME = ((1000U - HP303B__BUSYTIME_FAILSAFE) * HP303B__BUSYTIME_SCALING);
static const uint8_t HP303B__NUM_OF_SCAL_FACTS = 8;

static const uint8_t HP303B__SUCCEEDED = 0;
static const uint8_t HP303B__FAIL_UNKNOWN = -1;
static const uint8_t HP303B__FAIL_INIT_FAILED = -2;
static const uint8_t HP303B__FAIL_TOOBUSY = -3;
static const uint8_t HP303B__FAIL_UNFINISHED = -4;

// Constants for register manipulation
// SPI mode (3 or 4 wire)
static const uint8_t HP303B__REG_ADR_SPI3W = 0x09U;
static const uint8_t HP303B__REG_CONTENT_SPI3W = 0x01U;

// product id
static const uint8_t HP303B__REG_INFO_PROD_ID = HP303B__REG_ADR_PROD_ID, HP303B__REG_MASK_PROD_ID,
                     HP303B__REG_SHIFT_PROD_ID;
static const uint8_t HP303B__REG_ADR_PROD_ID = 0x0DU;
static const uint8_t HP303B__REG_MASK_PROD_ID = 0x0FU;
static const uint8_t HP303B__REG_SHIFT_PROD_ID = 0U;

// revision id
static const uint8_t HP303B__REG_INFO_REV_ID = HP303B__REG_ADR_REV_ID, HP303B__REG_MASK_REV_ID,
                     HP303B__REG_SHIFT_REV_ID;
static const uint8_t HP303B__REG_ADR_REV_ID = 0x0DU;
static const uint8_t HP303B__REG_MASK_REV_ID = 0xF0U;
static const uint8_t HP303B__REG_SHIFT_REV_ID = 4U;

// operating mode
static const uint8_t HP303B__REG_INFO_OPMODE = HP303B__REG_ADR_OPMODE, HP303B__REG_MASK_OPMODE,
                     HP303B__REG_SHIFT_OPMODE;
static const uint8_t HP303B__REG_ADR_OPMODE = 0x08U;
static const uint8_t HP303B__REG_MASK_OPMODE = 0x07U;
static const uint8_t HP303B__REG_SHIFT_OPMODE = 0U;

// temperature measure rate
static const uint8_t HP303B__REG_INFO_TEMP_MR = HP303B__REG_ADR_TEMP_MR, HP303B__REG_MASK_TEMP_MR,
                     HP303B__REG_SHIFT_TEMP_MR;
static const uint8_t HP303B__REG_ADR_TEMP_MR = 0x07U;
static const uint8_t HP303B__REG_MASK_TEMP_MR = 0x70U;
static const uint8_t HP303B__REG_SHIFT_TEMP_MR = 4U;

// temperature oversampling rate
static const uint8_t HP303B__REG_INFO_TEMP_OSR = HP303B__REG_ADR_TEMP_OSR, HP303B__REG_MASK_TEMP_OSR,
                     HP303B__REG_SHIFT_TEMP_OSR;
static const uint8_t HP303B__REG_ADR_TEMP_OSR = 0x07U;
static const uint8_t HP303B__REG_MASK_TEMP_OSR = 0x07U;
static const uint8_t HP303B__REG_SHIFT_TEMP_OSR = 0U;

// temperature sensor
static const uint8_t HP303B__REG_INFO_TEMP_SENSOR = HP303B__REG_ADR_TEMP_SENSOR, HP303B__REG_MASK_TEMP_SENSOR,
                     HP303B__REG_SHIFT_TEMP_SENSOR;
static const uint8_t HP303B__REG_ADR_TEMP_SENSOR = 0x07U;
static const uint8_t HP303B__REG_MASK_TEMP_SENSOR = 0x80U;
static const uint8_t HP303B__REG_SHIFT_TEMP_SENSOR = 7U;

// temperature sensor recommendation
static const uint8_t HP303B__REG_INFO_TEMP_SENSORREC = HP303B__REG_ADR_TEMP_SENSORREC, HP303B__REG_MASK_TEMP_SENSORREC,
                     HP303B__REG_SHIFT_TEMP_SENSORREC;
static const uint8_t HP303B__REG_ADR_TEMP_SENSORREC = 0x28U;
static const uint8_t HP303B__REG_MASK_TEMP_SENSORREC = 0x80U;
static const uint8_t HP303B__REG_SHIFT_TEMP_SENSORREC = 7U;

// temperature shift enable (if temp_osr>3)
static const uint8_t HP303B__REG_INFO_TEMP_SE = HP303B__REG_ADR_TEMP_SE, HP303B__REG_MASK_TEMP_SE,
                     HP303B__REG_SHIFT_TEMP_SE;
static const uint8_t HP303B__REG_ADR_TEMP_SE = 0x09U;
static const uint8_t HP303B__REG_MASK_TEMP_SE = 0x08U;
static const uint8_t HP303B__REG_SHIFT_TEMP_SE = 3U;

// pressure measure rate
static const uint8_t HP303B__REG_INFO_PRS_MR = HP303B__REG_ADR_PRS_MR, HP303B__REG_MASK_PRS_MR,
                     HP303B__REG_SHIFT_PRS_MR;
static const uint8_t HP303B__REG_ADR_PRS_MR = 0x06U;
static const uint8_t HP303B__REG_MASK_PRS_MR = 0x70U;
static const uint8_t HP303B__REG_SHIFT_PRS_MR = 4U;

// pressure oversampling rate
static const uint8_t HP303B__REG_INFO_PRS_OSR = HP303B__REG_ADR_PRS_OSR, HP303B__REG_MASK_PRS_OSR,
                     HP303B__REG_SHIFT_PRS_OSR;
static const uint8_t HP303B__REG_ADR_PRS_OSR = 0x06U;
static const uint8_t HP303B__REG_MASK_PRS_OSR = 0x07U;
static const uint8_t HP303B__REG_SHIFT_PRS_OSR = 0U;

// pressure shift enable (if prs_osr>3)
static const uint8_t HP303B__REG_INFO_PRS_SE = HP303B__REG_ADR_PRS_SE, HP303B__REG_MASK_PRS_SE,
                     HP303B__REG_SHIFT_PRS_SE;
static const uint8_t HP303B__REG_ADR_PRS_SE = 0x09U;
static const uint8_t HP303B__REG_MASK_PRS_SE = 0x04U;
static const uint8_t HP303B__REG_SHIFT_PRS_SE = 2U;

// temperature ready flag
static const uint8_t HP303B__REG_INFO_TEMP_RDY = HP303B__REG_ADR_TEMP_RDY, HP303B__REG_MASK_TEMP_RDY,
                     HP303B__REG_SHIFT_TEMP_RDY;
static const uint8_t HP303B__REG_ADR_TEMP_RDY = 0x08U;
static const uint8_t HP303B__REG_MASK_TEMP_RDY = 0x20U;
static const uint8_t HP303B__REG_SHIFT_TEMP_RDY = 5U;

// pressure ready flag
static const uint8_t HP303B__REG_INFO_PRS_RDY = HP303B__REG_ADR_PRS_RDY, HP303B__REG_MASK_PRS_RDY,
                     HP303B__REG_SHIFT_PRS_RDY;
static const uint8_t HP303B__REG_ADR_PRS_RDY = 0x08U;
static const uint8_t HP303B__REG_MASK_PRS_RDY = 0x10U;
static const uint8_t HP303B__REG_SHIFT_PRS_RDY = 4U;

// pressure value
static const uint8_t HP303B__REG_ADR_PRS = 0x00U;
static const uint8_t HP303B__REG_LEN_PRS = 3U;

// temperature value
static const uint8_t HP303B__REG_ADR_TEMP = 0x03U;
static const uint8_t HP303B__REG_LEN_TEMP = 3U;

// compensation coefficients
static const uint8_t HP303B__REG_ADR_COEF = 0x10U;
static const uint8_t HP303B__REG_LEN_COEF = 18;

// FIFO enable
static const uint8_t HP303B__REG_INFO_FIFO_EN = HP303B__REG_ADR_FIFO_EN, HP303B__REG_MASK_FIFO_EN,
                     HP303B__REG_SHIFT_FIFO_EN;
static const uint8_t HP303B__REG_ADR_FIFO_EN = 0x09U;
static const uint8_t HP303B__REG_MASK_FIFO_EN = 0x02U;
static const uint8_t HP303B__REG_SHIFT_FIFO_EN = 1U;

// FIFO flush
static const uint8_t HP303B__REG_INFO_FIFO_FL = HP303B__REG_ADR_FIFO_EN, HP303B__REG_MASK_FIFO_EN,
                     HP303B__REG_SHIFT_FIFO_EN;
static const uint8_t HP303B__REG_ADR_FIFO_FL = 0x0CU;
static const uint8_t HP303B__REG_MASK_FIFO_FL = 0x80U;
static const uint8_t HP303B__REG_SHIFT_FIFO_FL = 7U;

// FIFO empty
static const uint8_t HP303B__REG_INFO_FIFO_EMPTY = HP303B__REG_ADR_FIFO_EMPTY, HP303B__REG_MASK_FIFO_EMPTY,
                     HP303B__REG_SHIFT_FIFO_EMPTY;
static const uint8_t HP303B__REG_ADR_FIFO_EMPTY = 0x0BU;
static const uint8_t HP303B__REG_MASK_FIFO_EMPTY = 0x01U;
static const uint8_t HP303B__REG_SHIFT_FIFO_EMPTY = 0U;

// FIFO full
static const uint8_t HP303B__REG_INFO_FIFO_FULL = HP303B__REG_ADR_FIFO_FULL, HP303B__REG_MASK_FIFO_FULL,
                     HP303B__REG_SHIFT_FIFO_FULL;
static const uint8_t HP303B__REG_ADR_FIFO_FULL = 0x0BU;
static const uint8_t HP303B__REG_MASK_FIFO_FULL = 0x02U;
static const uint8_t HP303B__REG_SHIFT_FIFO_FULL = 1U;

// INT HL
static const uint8_t HP303B__REG_INFO_INT_HL = HP303B__REG_ADR_INT_HL, HP303B__REG_MASK_INT_HL,
                     HP303B__REG_SHIFT_INT_HL;
static const uint8_t HP303B__REG_ADR_INT_HL = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_HL = 0x80U;
static const uint8_t HP303B__REG_SHIFT_INT_HL = 7U;

// INT FIFO enable
static const uint8_t HP303B__REG_INFO_INT_EN_FIFO = HP303B__REG_ADR_INT_EN_FIFO, HP303B__REG_MASK_INT_EN_FIFO,
                     HP303B__REG_SHIFT_INT_EN_FIFO;
static const uint8_t HP303B__REG_ADR_INT_EN_FIFO = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_EN_FIFO = 0x40U;
static const uint8_t HP303B__REG_SHIFT_INT_EN_FIFO = 6U;

// INT TEMP enable
static const uint8_t HP303B__REG_INFO_INT_EN_TEMP = HP303B__REG_ADR_INT_EN_TEMP, HP303B__REG_MASK_INT_EN_TEMP,
                     HP303B__REG_SHIFT_INT_EN_TEMP;
static const uint8_t HP303B__REG_ADR_INT_EN_TEMP = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_EN_TEMP = 0x20U;
static const uint8_t HP303B__REG_SHIFT_INT_EN_TEMP = 5U;

// INT PRS enable
static const uint8_t HP303B__REG_INFO_INT_EN_PRS = HP303B__REG_ADR_INT_EN_PRS, HP303B__REG_MASK_INT_EN_PRS,
                     HP303B__REG_SHIFT_INT_EN_PRS;
static const uint8_t HP303B__REG_ADR_INT_EN_PRS = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_EN_PRS = 0x10U;
static const uint8_t HP303B__REG_SHIFT_INT_EN_PRS = 4U;

// INT FIFO flag
static const uint8_t HP303B__REG_INFO_INT_FLAG_FIFO = HP303B__REG_ADR_INT_FLAG_FIFO, HP303B__REG_MASK_INT_FLAG_FIFO,
                     HP303B__REG_SHIFT_INT_FLAG_FIFO;
static const uint8_t HP303B__REG_ADR_INT_FLAG_FIFO = 0x0AU;
static const uint8_t HP303B__REG_MASK_INT_FLAG_FIFO = 0x04U;
static const uint8_t HP303B__REG_SHIFT_INT_FLAG_FIFO = 2U;

// INT TMP flag
static const uint8_t HP303B__REG_INFO_INT_FLAG_TEMP = HP303B__REG_ADR_INT_FLAG_TEMP, HP303B__REG_MASK_INT_FLAG_TEMP,
                     HP303B__REG_SHIFT_INT_FLAG_TEMP;
static const uint8_t HP303B__REG_ADR_INT_FLAG_TEMP = 0x0AU;
static const uint8_t HP303B__REG_MASK_INT_FLAG_TEMP = 0x02U;
static const uint8_t HP303B__REG_SHIFT_INT_FLAG_TEMP = 1U;

// INT PRS flag
static const uint8_t HP303B__REG_INFO_INT_FLAG_PRS = HP303B__REG_ADR_INT_FLAG_PRS, HP303B__REG_MASK_INT_FLAG_PRS,
                     HP303B__REG_SHIFT_INT_FLAG_PRS;
static const uint8_t HP303B__REG_ADR_INT_FLAG_PRS = 0x0AU;
static const uint8_t HP303B__REG_MASK_INT_FLAG_PRS = 0x01U;
static const uint8_t HP303B__REG_SHIFT_INT_FLAG_PRS = 0U;

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
  int16_t write_byte_bitfield(uint8_t data, uint8_t reg_address, uint8_t mask, uint8_t shift, uint8_t check);
  int16_t write_byte_bitfield(uint8_t data, uint8_t reg_address, uint8_t mask, uint8_t shift);
  int16_t write_byte_spi(uint8_t reg_address, uint8_t data, uint8_t check);

  int16_t write_byte(uint8_t reg_address, uint8_t data, uint8_t check);
  int16_t write_byte(uint8_t reg_address, uint8_t data);
  int16_t read_block_sPI(uint8_t reg_address, uint8_t length, uint8_t *buffer);
  int16_t read_block(uint8_t reg_address, uint8_t length, uint8_t *buffer);
  int16_t read_byte_sPI(uint8_t reg_address);
  int16_t read_byte(uint8_t reg_address);
  int32_t calc_pressure(int32_t raw);
  int32_t calc_temp(int32_t raw);
  int16_t getFIFOvalue(int32_t *value);
  int16_t get_pressure(int32_t *result);
  int16_t get_temp(int32_t *result);
  uint16_t calc_busy_time(uint16_t mr, uint16_t osr);
  int16_t config_pressure(uint8_t prsMr, uint8_t prsOsr);
  int16_t config_temp(uint8_t tempMr, uint8_t tempOsr);

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