// SPA06

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::spa06_base {

// Read sizes. All other registers are size 1
static const size_t SPA06_MEAS_LEN = 3;
static const size_t SPA06_COEF_LEN = 21;

// Soft reset command (0b1001, 0x9)
static const uint8_t SPA06_SOFT_RESET = 0x9;

// SPA06 Register Addresses
enum {
  SPA06_PSR = 0x00,          // Pressure Reading MSB (or all 3)
  SPA06_PSR_B1 = 0x01,       // Pressure Reading LSB
  SPA06_PSR_B0 = 0x02,       // Pressure Reading XLSB (LSB: Pressure flag in FIFO)
  SPA06_TMP = 0x03,          // Temperature Reading MSB (or all 3)
  SPA06_TMP_B1 = 0x04,       // Temperature Reading LSB
  SPA06_TMP_B0 = 0x05,       // Temperature Reading XLSB
  SPA06_PSR_CFG = 0x06,      // Pressure Configuration
  SPA06_TMP_CFG = 0x07,      // Temperature Configuration
  SPA06_MEAS_CFG = 0x08,     // Measurement Configuration (includes readiness)
  SPA06_CFG_REG = 0x09,      // Configuration Register
  SPA06_INT_STS = 0x0A,      // Interrupt Status
  SPA06_FIFO_STS = 0x0B,     // FIFO Status
  SPA06_RESET = 0x0C,        // Reset + FIFO Flush
  SPA06_ID = 0x0D,           // Product ID and revision
  SPA06_COEF = 0x10,         // Coefficients (0x10-0x24)
  SPA06_INVALID_CMD = 0x25,  // End of enum command
};

// Oversampling config.
enum Oversampling {
  OVERSAMPLING_NONE = 0x0,
  OVERSAMPLING_X2 = 0x1,
  OVERSAMPLING_X4 = 0x2,
  OVERSAMPLING_X8 = 0x3,
  OVERSAMPLING_X16 = 0x4,
  OVERSAMPLING_X32 = 0x5,
  OVERSAMPLING_X64 = 0x6,
  OVERSAMPLING_X128 = 0x7,
  OVERSAMPLING_COUNT = 0x8,
};

// Measuring rate config
enum SampleRate {
  SAMPLE_RATE_1 = 0x0,
  SAMPLE_RATE_2 = 0x1,
  SAMPLE_RATE_4 = 0x2,
  SAMPLE_RATE_8 = 0x3,
  SAMPLE_RATE_16 = 0x4,
  SAMPLE_RATE_32 = 0x5,
  SAMPLE_RATE_64 = 0x6,
  SAMPLE_RATE_128 = 0x7,
  SAMPLE_RATE_25P8 = 0x9,
  SAMPLE_RATE_25P4 = 0xA,
  SAMPLE_RATE_25P2 = 0xB,
  SAMPLE_RATE_25 = 0xC,
  SAMPLE_RATE_50 = 0xD,
  SAMPLE_RATE_100 = 0xE,
  SAMPLE_RATE_200 = 0xF,
};

// Measuring control config, set in MEAS_CFG register.
// See datasheet pages 28-29

enum MeasCrtl {
  MEASCRTL_IDLE = 0x0,
  MEASCRTL_PRES = 0x1,
  MEASCRTL_TEMP = 0x2,
  MEASCRTL_BG_PRES = 0x5,
  MEASCRTL_BG_TEMP = 0x6,
  MEASCRTL_BG_BOTH = 0x7,
};

// Oversampling scale factors. See datasheet page 15.
static const uint32_t oversampling_to_scale_factor(const Oversampling oversampling) {
  switch (oversampling) {
    case Oversampling::OVERSAMPLING_NONE:
      return 524288;
    case Oversampling::OVERSAMPLING_X2:
      return 1572864;
    case Oversampling::OVERSAMPLING_X4:
      return 3670016;
    case Oversampling::OVERSAMPLING_X8:
      return 7864320;
    case Oversampling::OVERSAMPLING_X16:
      return 253952;
    case Oversampling::OVERSAMPLING_X32:
      return 516096;
    case Oversampling::OVERSAMPLING_X64:
      return 1040384;
    case Oversampling::OVERSAMPLING_X128:
      return 2088960;
    default:
      return 524288;
  }
};

static const LogString *oversampling_to_str(const Oversampling oversampling) {
  switch (oversampling) {
    case Oversampling::OVERSAMPLING_NONE:
      return LOG_STR("X1");
    case Oversampling::OVERSAMPLING_X2:
      return LOG_STR("X2");
    case Oversampling::OVERSAMPLING_X4:
      return LOG_STR("X4");
    case Oversampling::OVERSAMPLING_X8:
      return LOG_STR("X8");
    case Oversampling::OVERSAMPLING_X16:
      return LOG_STR("X16");
    case Oversampling::OVERSAMPLING_X32:
      return LOG_STR("X32");
    case Oversampling::OVERSAMPLING_X64:
      return LOG_STR("X64");
    case Oversampling::OVERSAMPLING_X128:
      return LOG_STR("X128");
    default:
      return LOG_STR("");
  }
};

static const LogString *meas_rate_to_str(SampleRate rate) {
  switch (rate) {
    case SampleRate::SAMPLE_RATE_1:
      return LOG_STR("1Hz");
    case SampleRate::SAMPLE_RATE_2:
      return LOG_STR("2Hz");
    case SampleRate::SAMPLE_RATE_4:
      return LOG_STR("4Hz");
    case SampleRate::SAMPLE_RATE_8:
      return LOG_STR("8Hz");
    case SampleRate::SAMPLE_RATE_16:
      return LOG_STR("16Hz");
    case SampleRate::SAMPLE_RATE_32:
      return LOG_STR("32Hz");
    case SampleRate::SAMPLE_RATE_64:
      return LOG_STR("64Hz");
    case SampleRate::SAMPLE_RATE_128:
      return LOG_STR("128Hz");
    case SampleRate::SAMPLE_RATE_25P8:
      return LOG_STR("3.125Hz");
    case SampleRate::SAMPLE_RATE_25P4:
      return LOG_STR("6.25Hz");
    case SampleRate::SAMPLE_RATE_25P2:
      return LOG_STR("12.5Hz");
    case SampleRate::SAMPLE_RATE_25:
      return LOG_STR("25Hz");
    case SampleRate::SAMPLE_RATE_50:
      return LOG_STR("50Hz");
    case SampleRate::SAMPLE_RATE_100:
      return LOG_STR("100Hz");
    case SampleRate::SAMPLE_RATE_200:
      return LOG_STR("200Hz");
    default:
      return LOG_STR("");
  }
}

class SPA06Component : public PollingComponent {
 public:
  //// Standard ESPHome component class functions
  void setup() override;
  void update() override;
  void dump_config() override;

  //// ESPHome-side settings
  void set_conversion_time(uint8_t conversion_time) { this->conversion_time_ = conversion_time; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }
  void set_temperature_oversampling_config(Oversampling temperature_oversampling) {
    this->temperature_oversampling_ = temperature_oversampling;
    this->kt_ = oversampling_to_scale_factor(temperature_oversampling);
  }
  void set_pressure_oversampling_config(Oversampling pressure_oversampling) {
    this->pressure_oversampling_ = pressure_oversampling;
    this->kp_ = oversampling_to_scale_factor(pressure_oversampling);
  }
  void set_pressure_sample_rate_config(SampleRate rate) { this->pressure_rate_ = rate; }
  void set_temperature_sample_rate_config(SampleRate rate) { this->temperature_rate_ = rate; }

 protected:
  // Virtual read functions. Implemented in SPI/I2C components
  virtual bool spa_read_byte(uint8_t reg, uint8_t *data) = 0;
  virtual bool spa_write_byte(uint8_t reg, uint8_t data) = 0;
  virtual bool spa_read_bytes(uint8_t reg, uint8_t *data, size_t len) = 0;
  virtual bool spa_write_bytes(uint8_t reg, uint8_t *data, size_t len) = 0;

  //// Protocol-specific read functions
  // Soft reset
  bool soft_reset_();
  // Protocol-specific reset (used for SPI only, implemented as noop for I2C)
  virtual void protocol_reset_() = 0;
  // Read temperature and calculate Celsius and scaled raw temperatures
  bool read_temperature_(float &temperature, float &t_raw_sc);
  // No pressure only read! Pressure calculation depends on scaled temperature value
  // Read temperature and calculate Celsius temperature, Pascal pressure, and scaled raw temperature
  bool read_temperature_and_pressure_(float &temperature, float &pressure, float &t_raw_sc);
  // Read coefficients. Stores in class variables.
  bool read_coefficients_();

  //// Protocol-specific write functions
  // Write temperature settings to TMP_CFG register
  bool write_temperature_settings_(Oversampling oversampling, SampleRate rate);
  // Write pressure settings to PRS_CFG register
  bool write_pressure_settings_(Oversampling oversampling, SampleRate rate);
  // Write measurement settings to MEAS_CRTL register
  bool write_measurement_settings_(MeasCrtl crtl);

  // Write communication settings to CFG_REG register
  // Set pressure_shift to true if pressure oversampling >X8
  // Set temperature_shift to true if temperature oversampling >X8
  bool write_communication_settings_(bool pressure_shift, bool temperature_shift, bool interrupt_hl = false,
                                     bool interrupt_fifo = false, bool interrupt_tmp = false,
                                     bool interrupt_prs = false, bool enable_fifo = false, bool spi_3wire = false);

  //// Protocol helper functions
  // Write function for both temperature and pressure (deduplicates code)
  bool write_sensor_settings_(Oversampling oversampling, SampleRate rate, uint8_t reg);
  // Convert raw temperature reading into Celsius
  float convert_temperature_(const float &t_raw_sc);
  // Convert raw pressure and scaled raw temperature into Pascals
  float convert_pressure_(const float &p_raw_sc, const float &t_raw_sc);

  //// Protocol-related variables
  // Oversampling scale factors
  uint32_t kp_{0}, kt_{0};
  // Coefficients for calculating pressure and temperature from raw values
  // Obtained from IC during setup
  int32_t c00_{0}, c10_{0};
  int16_t c0_{0}, c1_{0}, c01_{0}, c11_{0}, c20_{0}, c21_{0}, c30_{0}, c31_{0}, c40_{0};

  //// ESPHome class objects and configuration
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  Oversampling temperature_oversampling_{Oversampling::OVERSAMPLING_NONE};
  Oversampling pressure_oversampling_{Oversampling::OVERSAMPLING_NONE};
  SampleRate temperature_rate_{SampleRate::SAMPLE_RATE_8};
  SampleRate pressure_rate_{SampleRate::SAMPLE_RATE_4};
  uint8_t conversion_time_{10};

  // Protocol-related union-structs
  union {
    struct {
      uint32_t data : 24;
    } val;
    uint8_t reg[3];
  } psr_tmp_read_ = {.reg = 0};  // PSR and TMP read

  union {
    struct {
      Oversampling prc : 4;
      SampleRate rate : 4;
    } bit;
    uint8_t reg;
  } pt_meas_cfg_ = {.reg = 0};  // PRS_CFG and TMP_CFG

  union {
    struct {
      uint8_t meas_crtl : 3;
      bool tmp_ext : 1;
      bool prs_ready : 1;
      bool tmp_ready : 1;
      bool sensor_ready : 1;
      bool coef_ready : 1;
    } bit;
    uint8_t reg;
  } meas_ = {.reg = 0};  // MEAS_REG

  union {
    struct {
      uint8_t _reserved : 5;
      bool int_prs : 1;
      bool int_tmp : 1;
      bool int_fifo_full : 1;
    } bit;
    uint8_t reg;
  } int_status_ = {.reg = 0};  // INT_STS

  union {
    struct {
      bool spi_3wire : 1;
      bool fifo_en : 1;
      bool p_shift : 1;
      bool t_shift : 1;
      bool int_prs : 1;
      bool int_tmp : 1;
      bool int_fifo : 1;
      bool int_hl : 1;
    } bit;
    uint8_t reg;
  } cfg_ = {.reg = 0};  // CFG_REG

  union {
    struct {
      bool fifo_empty : 1;
      bool fifo_full : 1;
      uint8_t _reserved : 6;
    } bit;
    uint8_t reg;
  } fifo_sts_ = {.reg = 0};  // FIFO_STS

  union {
    struct {
      // Set to true to flush FIFO
      bool fifo_flush : 1;
      // Reserved bits
      uint8_t _reserved : 3;
      // Soft reset. Set to 1001 (0x9) to perform reset.
      uint8_t soft_rst : 4;
    } bit;
    uint8_t reg = 0;
  } reset_ = {.reg = 0};  // RESET

  union {
    struct {
      uint8_t prod_id : 4;
      uint8_t rev_id : 4;
    } bit;
    uint8_t reg = 0;
  } prod_id_ = {.reg = 0};  // ID

};  // class SPA06Component

}  // namespace esphome::spa06_base
