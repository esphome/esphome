#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace hp303b {
static const char *TAG = "hp303b.sensor";

// general Constants
static const int16_t HP303B__PROD_ID = 0U;

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
static const uint16_t HP303B__MAX_BUSYTIME = ((1000 - HP303B__BUSYTIME_FAILSAFE) * HP303B__BUSYTIME_SCALING);
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
#define HP303B__REG_ADR_PROD_ID 0x0DU
#define HP303B__REG_MASK_PROD_ID 0x0FU
#define HP303B__REG_SHIFT_PROD_ID 0U
#define HP303B__REG_INFO_PROD_ID HP303B__REG_ADR_PROD_ID, HP303B__REG_MASK_PROD_ID, HP303B__REG_SHIFT_PROD_ID
// revision id
#define HP303B__REG_ADR_REV_ID 0x0DU
#define HP303B__REG_MASK_REV_ID 0xF0U
#define HP303B__REG_SHIFT_REV_ID 4U
#define HP303B__REG_INFO_REV_ID HP303B__REG_ADR_REV_ID, HP303B__REG_MASK_REV_ID, HP303B__REG_SHIFT_REV_ID

// operating mode
#define HP303B__REG_ADR_OPMODE 0x08U
#define HP303B__REG_MASK_OPMODE 0x07U
#define HP303B__REG_SHIFT_OPMODE 0U
#define HP303B__REG_INFO_OPMODE HP303B__REG_ADR_OPMODE, HP303B__REG_MASK_OPMODE, HP303B__REG_SHIFT_OPMODE

// temperature measure rate
#define HP303B__REG_ADR_TEMP_MR 0x07U
#define HP303B__REG_MASK_TEMP_MR 0x70U
#define HP303B__REG_SHIFT_TEMP_MR 4U
#define HP303B__REG_INFO_TEMP_MR HP303B__REG_ADR_TEMP_MR, HP303B__REG_MASK_TEMP_MR, HP303B__REG_SHIFT_TEMP_MR

// temperature oversampling rate
#define HP303B__REG_ADR_TEMP_OSR 0x07U
#define HP303B__REG_MASK_TEMP_OSR 0x07U
#define HP303B__REG_SHIFT_TEMP_OSR 0U
#define HP303B__REG_INFO_TEMP_OSR HP303B__REG_ADR_TEMP_OSR, HP303B__REG_MASK_TEMP_OSR, HP303B__REG_SHIFT_TEMP_OSR

// temperature sensor
#define HP303B__REG_ADR_TEMP_SENSOR 0x07U
#define HP303B__REG_MASK_TEMP_SENSOR 0x80U
#define HP303B__REG_SHIFT_TEMP_SENSOR 7U
#define HP303B__REG_INFO_TEMP_SENSOR \
  HP303B__REG_ADR_TEMP_SENSOR, HP303B__REG_MASK_TEMP_SENSOR, HP303B__REG_SHIFT_TEMP_SENSOR

// temperature sensor recommendation
#define HP303B__REG_ADR_TEMP_SENSORREC 0x28U
#define HP303B__REG_MASK_TEMP_SENSORREC 0x80U
#define HP303B__REG_SHIFT_TEMP_SENSORREC 7U
#define HP303B__REG_INFO_TEMP_SENSORREC \
  HP303B__REG_ADR_TEMP_SENSORREC, HP303B__REG_MASK_TEMP_SENSORREC, HP303B__REG_SHIFT_TEMP_SENSORREC

// temperature shift enable (if temp_osr>3)
#define HP303B__REG_ADR_TEMP_SE 0x09U
#define HP303B__REG_MASK_TEMP_SE 0x08U
#define HP303B__REG_SHIFT_TEMP_SE 3U
#define HP303B__REG_INFO_TEMP_SE HP303B__REG_ADR_TEMP_SE, HP303B__REG_MASK_TEMP_SE, HP303B__REG_SHIFT_TEMP_SE

// pressure measure rate
#define HP303B__REG_ADR_PRS_MR 0x06U
#define HP303B__REG_MASK_PRS_MR 0x70U
#define HP303B__REG_SHIFT_PRS_MR 4U
#define HP303B__REG_INFO_PRS_MR HP303B__REG_ADR_PRS_MR, HP303B__REG_MASK_PRS_MR, HP303B__REG_SHIFT_PRS_MR

// pressure oversampling rate
#define HP303B__REG_ADR_PRS_OSR 0x06U
#define HP303B__REG_MASK_PRS_OSR 0x07U
#define HP303B__REG_SHIFT_PRS_OSR 0U
#define HP303B__REG_INFO_PRS_OSR HP303B__REG_ADR_PRS_OSR, HP303B__REG_MASK_PRS_OSR, HP303B__REG_SHIFT_PRS_OSR

// pressure shift enable (if prs_osr>3)
#define HP303B__REG_ADR_PRS_SE 0x09U
#define HP303B__REG_MASK_PRS_SE 0x04U
#define HP303B__REG_SHIFT_PRS_SE 2U
#define HP303B__REG_INFO_PRS_SE HP303B__REG_ADR_PRS_SE, HP303B__REG_MASK_PRS_SE, HP303B__REG_SHIFT_PRS_SE

// temperature ready flag
#define HP303B__REG_ADR_TEMP_RDY 0x08U
#define HP303B__REG_MASK_TEMP_RDY 0x20U
#define HP303B__REG_SHIFT_TEMP_RDY 5U
#define HP303B__REG_INFO_TEMP_RDY HP303B__REG_ADR_TEMP_RDY, HP303B__REG_MASK_TEMP_RDY, HP303B__REG_SHIFT_TEMP_RDY

// pressure ready flag
#define HP303B__REG_ADR_PRS_RDY 0x08U
#define HP303B__REG_MASK_PRS_RDY 0x10U
#define HP303B__REG_SHIFT_PRS_RDY 4U
#define HP303B__REG_INFO_PRS_RDY HP303B__REG_ADR_PRS_RDY, HP303B__REG_MASK_PRS_RDY, HP303B__REG_SHIFT_PRS_RDY

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
static const uint8_t HP303B__REG_ADR_FIFO_EN = 0x09U;
static const uint8_t HP303B__REG_MASK_FIFO_EN = 0x02U;
static const uint8_t HP303B__REG_SHIFT_FIFO_EN = 1U;
static const uint8_t HP303B__REG_INFO_FIFO_EN = HP303B__REG_ADR_FIFO_EN, HP303B__REG_MASK_FIFO_EN,
                     HP303B__REG_SHIFT_FIFO_EN;

// FIFO flush
static const uint8_t HP303B__REG_ADR_FIFO_FL = 0x0CU;
static const uint8_t HP303B__REG_MASK_FIFO_FL = 0x80U;
static const uint8_t HP303B__REG_SHIFT_FIFO_FL = 7U;
static const uint8_t HP303B__REG_INFO_FIFO_FL = HP303B__REG_ADR_FIFO_EN, HP303B__REG_MASK_FIFO_EN,
                     HP303B__REG_SHIFT_FIFO_EN;

// FIFO empty
static const uint8_t HP303B__REG_ADR_FIFO_EMPTY = 0x0BU;
static const uint8_t HP303B__REG_MASK_FIFO_EMPTY = 0x01U;
static const uint8_t HP303B__REG_SHIFT_FIFO_EMPTY = 0U;
static const uint8_t HP303B__REG_INFO_FIFO_EMPTY = HP303B__REG_ADR_FIFO_EMPTY, HP303B__REG_MASK_FIFO_EMPTY,
                     HP303B__REG_SHIFT_FIFO_EMPTY;

// FIFO full
#define HP303B__REG_ADR_FIFO_FULL = 0x0BU;
#define HP303B__REG_MASK_FIFO_FULL = 0x02U;
#define HP303B__REG_SHIFT_FIFO_FULL = 1U;
#define HP303B__REG_INFO_FIFO_FULL HP303B__REG_ADR_FIFO_FULL, HP303B__REG_MASK_FIFO_FULL, HP303B__REG_SHIFT_FIFO_FULL;

// INT HL
static const uint8_t HP303B__REG_ADR_INT_HL = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_HL = 0x80U;
static const uint8_t HP303B__REG_SHIFT_INT_HL = 7U;
static const uint8_t HP303B__REG_INFO_INT_HL = HP303B__REG_ADR_INT_HL, HP303B__REG_MASK_INT_HL,
                     HP303B__REG_SHIFT_INT_HL;

// INT FIFO enable
static const uint8_t HP303B__REG_ADR_INT_EN_FIFO = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_EN_FIFO = 0x40U;
static const uint8_t HP303B__REG_SHIFT_INT_EN_FIFO = 6U;
static const uint8_t HP303B__REG_INFO_INT_EN_FIFO = HP303B__REG_ADR_INT_EN_FIFO, HP303B__REG_MASK_INT_EN_FIFO,
                     HP303B__REG_SHIFT_INT_EN_FIFO;

// INT TEMP enable
static const uint8_t HP303B__REG_ADR_INT_EN_TEMP = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_EN_TEMP = 0x20U;
static const uint8_t HP303B__REG_SHIFT_INT_EN_TEMP = 5U;
static const uint8_t HP303B__REG_INFO_INT_EN_TEMP = HP303B__REG_ADR_INT_EN_TEMP, HP303B__REG_MASK_INT_EN_TEMP,
                     HP303B__REG_SHIFT_INT_EN_TEMP;

// INT PRS enable
static const uint8_t HP303B__REG_ADR_INT_EN_PRS = 0x09U;
static const uint8_t HP303B__REG_MASK_INT_EN_PRS = 0x10U;
static const uint8_t HP303B__REG_SHIFT_INT_EN_PRS = 4U;
static const uint8_t HP303B__REG_INFO_INT_EN_PRS = HP303B__REG_ADR_INT_EN_PRS, HP303B__REG_MASK_INT_EN_PRS,
                     HP303B__REG_SHIFT_INT_EN_PRS;

// INT FIFO flag
static const uint8_t HP303B__REG_ADR_INT_FLAG_FIFO = 0x0AU;
static const uint8_t HP303B__REG_MASK_INT_FLAG_FIFO = 0x04U;
static const uint8_t HP303B__REG_SHIFT_INT_FLAG_FIFO = 2U;
static const uint8_t HP303B__REG_INFO_INT_FLAG_FIFO = HP303B__REG_ADR_INT_FLAG_FIFO, HP303B__REG_MASK_INT_FLAG_FIFO,
                     HP303B__REG_SHIFT_INT_FLAG_FIFO;

// INT TMP flag
static const uint8_t HP303B__REG_ADR_INT_FLAG_TEMP = 0x0AU;
static const uint8_t HP303B__REG_MASK_INT_FLAG_TEMP = 0x02U;
static const uint8_t HP303B__REG_SHIFT_INT_FLAG_TEMP = 1U;
static const uint8_t HP303B__REG_INFO_INT_FLAG_TEMP = HP303B__REG_ADR_INT_FLAG_TEMP, HP303B__REG_MASK_INT_FLAG_TEMP,
                     HP303B__REG_SHIFT_INT_FLAG_TEMP;

// INT PRS flag
static const uint8_t HP303B__REG_ADR_INT_FLAG_PRS = 0x0AU;
static const uint8_t HP303B__REG_MASK_INT_FLAG_PRS = 0x01U;
static const uint8_t HP303B__REG_SHIFT_INT_FLAG_PRS = 0U;
static const uint8_t HP303B__REG_INFO_INT_FLAG_PRS = HP303B__REG_ADR_INT_FLAG_PRS, HP303B__REG_MASK_INT_FLAG_PRS,
                     HP303B__REG_SHIFT_INT_FLAG_PRS;

class HP303BComponent : public PollingComponent {
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

  virtual int set_interrupt_polarity(uint8_t polarity) = 0;

  int16_t measure_pressureOnce(int32_t &result, uint8_t oversampling_rate);
  int16_t measure_pressureOnce(int32_t &result) { return measurePressureOnce(result, m_prs_osr); }
  int16_t start_measure_pressure_once() { return startMeasurePressureOnce(m_prs_osr); };
  int16_t start_measure_pressure_once(uint8_t over_sampling_rate);
  int16_t set_op_mode(uint8_t op_mode);
  int16_t read_byte_bitfield(uint8_t reg_address, uint8_t mask, uint8_t shift);
  int16_t write_byte_bitfield(uint8_t data, uint8_t reg_address, uint8_t mask, uint8_t shift, uint8_t check);
  int16_t write_byte_bitfield(uint8_t data, uint8_t reg_address, uint8_t mask, uint8_t shift);

  virtual int write_byte(uint8_t reg_address, uint8_t data, uint8_t check) = 0;
  int16_t write_byte(uint8_t reg_address, uint8_t data);
  virtual int read_block(uint8_t reg_address, uint8_t length, uint8_t *buffer) = 0;
  virtual int read_byte(uint8_t reg_address) = 0;
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
  static const int32_t scaling_facts[HP303B__NUM_OF_SCAL_FACTS] = {524288, 1572864, 3670016, 7864320,
                                                                   253952, 516096,  1040384, 2088960};

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
  Mode m_op_mode;

  // flags
  uint8_t m_init_fail;
  uint8_t m_product_id;
  uint8_t m_revision_id;

  // settings
  uint8_t m_temp_mr;
  uint8_t m_temp_osr;
  uint8_t m_prs_mr;
  uint8_t m_prs_osr;
  uint8_t m_temp_sensor;

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
  double m_last_tempS_scal;
}
}  // namespace hp303b
}  // namespace esphome