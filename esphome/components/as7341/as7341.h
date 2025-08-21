#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace as7341 {

static const uint8_t AS7341_CHIP_ID = 0x09;

static const uint8_t AS7341_CONFIG = 0x70;
static const uint8_t AS7341_LED = 0x74;

static const uint8_t AS7341_ENABLE = 0x80;
static const uint8_t AS7341_ATIME = 0x81;

static const uint8_t AS7341_WTIME = 0x83;

static const uint8_t AS7341_AUXID = 0x90;
static const uint8_t AS7341_REVID = 0x91;
static const uint8_t AS7341_ID = 0x92;
static const uint8_t AS7341_STATUS = 0x93;

static const uint8_t AS7341_CH0_DATA_L = 0x95;
static const uint8_t AS7341_CH0_DATA_H = 0x96;
static const uint8_t AS7341_CH1_DATA_L = 0x97;
static const uint8_t AS7341_CH1_DATA_H = 0x98;
static const uint8_t AS7341_CH2_DATA_L = 0x99;
static const uint8_t AS7341_CH2_DATA_H = 0x9A;
static const uint8_t AS7341_CH3_DATA_L = 0x9B;
static const uint8_t AS7341_CH3_DATA_H = 0x9C;
static const uint8_t AS7341_CH4_DATA_L = 0x9D;
static const uint8_t AS7341_CH4_DATA_H = 0x9E;
static const uint8_t AS7341_CH5_DATA_L = 0x9F;
static const uint8_t AS7341_CH5_DATA_H = 0xA0;

static const uint8_t AS7341_STATUS2 = 0xA3;

static const uint8_t AS7341_CFG1 = 0xAA;  ///< Controls ADC Gain

static const uint8_t AS7341_CFG6 = 0xAF;  // Stores SMUX command
static const uint8_t AS7341_CFG9 = 0xB2;  // Config for system interrupts (SMUX, Flicker detection)

static const uint8_t AS7341_ASTEP = 0xCA;      // LSB
static const uint8_t AS7341_ASTEP_MSB = 0xCB;  // MSB

enum AS7341AdcChannel {
  AS7341_ADC_CHANNEL_0,
  AS7341_ADC_CHANNEL_1,
  AS7341_ADC_CHANNEL_2,
  AS7341_ADC_CHANNEL_3,
  AS7341_ADC_CHANNEL_4,
  AS7341_ADC_CHANNEL_5,
};

enum AS7341SmuxCommand {
  AS7341_SMUX_CMD_ROM_RESET,  ///< ROM code initialization of SMUX
  AS7341_SMUX_CMD_READ,       ///< Read SMUX configuration to RAM from SMUX chain
  AS7341_SMUX_CMD_WRITE,      ///< Write SMUX configuration from RAM to SMUX chain
};

enum AS7341Gain {
  AS7341_GAIN_0_5X,
  AS7341_GAIN_1X,
  AS7341_GAIN_2X,
  AS7341_GAIN_4X,
  AS7341_GAIN_8X,
  AS7341_GAIN_16X,
  AS7341_GAIN_32X,
  AS7341_GAIN_64X,
  AS7341_GAIN_128X,
  AS7341_GAIN_256X,
  AS7341_GAIN_512X,
};

class AS7341Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_f1_sensor(sensor::Sensor *f1_sensor) { this->f1_ = f1_sensor; }
  void set_f2_sensor(sensor::Sensor *f2_sensor) { f2_ = f2_sensor; }
  void set_f3_sensor(sensor::Sensor *f3_sensor) { f3_ = f3_sensor; }
  void set_f4_sensor(sensor::Sensor *f4_sensor) { f4_ = f4_sensor; }
  void set_f5_sensor(sensor::Sensor *f5_sensor) { f5_ = f5_sensor; }
  void set_f6_sensor(sensor::Sensor *f6_sensor) { f6_ = f6_sensor; }
  void set_f7_sensor(sensor::Sensor *f7_sensor) { f7_ = f7_sensor; }
  void set_f8_sensor(sensor::Sensor *f8_sensor) { f8_ = f8_sensor; }
  void set_clear_sensor(sensor::Sensor *clear_sensor) { clear_ = clear_sensor; }
  void set_nir_sensor(sensor::Sensor *nir_sensor) { nir_ = nir_sensor; }

  void set_gain(AS7341Gain gain) { gain_ = gain; }
  void set_atime(uint8_t atime) { atime_ = atime; }
  void set_astep(uint16_t astep) { astep_ = astep; }

  AS7341Gain get_gain();
  uint8_t get_atime();
  uint16_t get_astep();
  bool setup_gain(AS7341Gain gain);
  bool setup_atime(uint8_t atime);
  bool setup_astep(uint16_t astep);

  uint16_t read_channel(AS7341AdcChannel channel);
  bool read_channels(uint16_t *data);
  void set_smux_low_channels(bool enable);
  bool set_smux_command(AS7341SmuxCommand command);
  void configure_smux_low_channels();
  void configure_smux_high_channels();
  bool enable_smux();

  bool wait_for_data();
  bool is_data_ready();
  bool enable_power(bool enable);
  bool enable_spectral_measurement(bool enable);

  bool read_register_bit(uint8_t address, uint8_t bit_position);
  bool write_register_bit(uint8_t address, bool value, uint8_t bit_position);
  bool set_register_bit(uint8_t address, uint8_t bit_position);
  bool clear_register_bit(uint8_t address, uint8_t bit_position);
  uint16_t swap_bytes(uint16_t data);

 protected:
  sensor::Sensor *f1_{nullptr};
  sensor::Sensor *f2_{nullptr};
  sensor::Sensor *f3_{nullptr};
  sensor::Sensor *f4_{nullptr};
  sensor::Sensor *f5_{nullptr};
  sensor::Sensor *f6_{nullptr};
  sensor::Sensor *f7_{nullptr};
  sensor::Sensor *f8_{nullptr};
  sensor::Sensor *clear_{nullptr};
  sensor::Sensor *nir_{nullptr};

  uint16_t astep_;
  AS7341Gain gain_;
  uint8_t atime_;
  uint16_t channel_readings_[12];
};

}  // namespace as7341
}  // namespace esphome
