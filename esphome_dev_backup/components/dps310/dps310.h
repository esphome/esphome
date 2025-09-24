#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace dps310 {

static const uint8_t DPS310_REG_PRS_B2 = 0x00;        // Highest byte of pressure data
static const uint8_t DPS310_REG_TMP_B2 = 0x03;        // Highest byte of temperature data
static const uint8_t DPS310_REG_PRS_CFG = 0x06;       // Pressure configuration
static const uint8_t DPS310_REG_TMP_CFG = 0x07;       // Temperature configuration
static const uint8_t DPS310_REG_MEAS_CFG = 0x08;      // Sensor configuration
static const uint8_t DPS310_REG_CFG = 0x09;           // Interrupt/FIFO configuration
static const uint8_t DPS310_REG_RESET = 0x0C;         // Soft reset
static const uint8_t DPS310_REG_PROD_REV_ID = 0x0D;   // Register that contains the part ID
static const uint8_t DPS310_REG_COEF = 0x10;          // Top of calibration coefficient data space
static const uint8_t DPS310_REG_TMP_COEF_SRC = 0x28;  // Temperature calibration src

static const uint8_t DPS310_BIT_PRS_RDY = 0x10;       // Pressure measurement is ready
static const uint8_t DPS310_BIT_TMP_RDY = 0x20;       // Temperature measurement is ready
static const uint8_t DPS310_BIT_SENSOR_RDY = 0x40;    // Sensor initialization complete when bit is set
static const uint8_t DPS310_BIT_COEF_RDY = 0x80;      // Coefficients are available when bit is set
static const uint8_t DPS310_BIT_TMP_COEF_SRC = 0x80;  // Temperature measurement source (0 = ASIC, 1 = MEMS element)
static const uint8_t DPS310_BIT_REQ_PRES = 0x01;      // Set this bit to request pressure reading
static const uint8_t DPS310_BIT_REQ_TEMP = 0x02;      // Set this bit to request temperature reading

static const uint8_t DPS310_CMD_RESET = 0x89;  // What to write to reset the device

static const uint8_t DPS310_VAL_PRS_CFG = 0x01;  // Value written to DPS310_REG_PRS_CFG at startup
static const uint8_t DPS310_VAL_TMP_CFG = 0x01;  // Value written to DPS310_REG_TMP_CFG at startup
static const uint8_t DPS310_VAL_REG_CFG = 0x00;  // Value written to DPS310_REG_CFG at startup

static const uint8_t DPS310_INIT_TIMEOUT = 20;       // How long to wait for DPS310 start-up to complete
static const uint8_t DPS310_NUM_COEF_REGS = 18;      // Number of coefficients we need to read from the device
static const int32_t DPS310_SCALE_FACTOR = 1572864;  // Measurement compensation scale factor

class DPS310Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

 protected:
  void read_();
  void read_pressure_();
  void read_temperature_();
  void calculate_values_(int32_t raw_temperature, int32_t raw_pressure);
  static int32_t twos_complement(int32_t val, uint8_t bits);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  int32_t raw_pressure_, raw_temperature_, c00_, c10_;
  int16_t c0_, c1_, c01_, c11_, c20_, c21_, c30_;
  uint8_t prod_rev_id_;
  bool got_pres_, got_temp_, update_in_progress_;
};

}  // namespace dps310
}  // namespace esphome
