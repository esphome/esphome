#pragma once

#include "esphome/components/mt6701/mt6701.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome::mt6701_i2c {

// Angle output registers: D[13:6] at 0x03, D[5:0] in bits 7:2 of 0x04.
static const uint8_t REG_ANGLE_H = 0x03;
static const uint8_t REG_ANGLE_L = 0x04;

// EEPROM programming sequence (datasheet section 8.2). The chip must be left
// alone for more than 600 ms afterwards, so the wait carries a margin.
static const uint8_t REG_EEPROM_KEY = 0x09;
static const uint8_t REG_EEPROM_CMD = 0x0A;
static const uint8_t EEPROM_KEY_VALUE = 0xB3;
static const uint8_t EEPROM_CMD_VALUE = 0x05;
static const uint32_t EEPROM_PROGRAM_DELAY_MS = 700;

// Configuration registers (writable over I2C only). Several of these also
// contain manufacturer-reserved bits, so every write is read-modify-write.
static const uint8_t REG_ABZ_MUX_DIR = 0x29;  // bit6 ABZ_MUX, bit1 DIR
static const uint8_t REG_RES_H = 0x30;        // bits7:4 UVW_RES, bits1:0 ABZ_RES[9:8]
static const uint8_t REG_ABZ_RES_L = 0x31;    // ABZ_RES[7:0]
static const uint8_t REG_CONFIG_H = 0x32;     // bit7 HYST[2], bits6:4 Z_PULSE_WIDTH, bits3:0 ZERO[11:8]
static const uint8_t REG_ZERO_L = 0x33;       // ZERO[7:0]
static const uint8_t REG_HYST_L = 0x34;       // bits7:6 HYST[1:0]
static const uint8_t REG_OUT = 0x38;          // bit7 PWM_FREQ, bit6 PWM_POL, bit5 OUT_MODE
static const uint8_t REG_A_HIGH = 0x3E;       // bits7:4 A_STOP[11:8], bits3:0 A_START[11:8]
static const uint8_t REG_A_START_L = 0x3F;    // A_START[7:0]
static const uint8_t REG_A_STOP_L = 0x40;     // A_STOP[7:0]

/// MT6701 driver over the I2C interface, with full configuration support.
class MT6701I2CComponent final : public mt6701::MT6701Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  // Configuration setters. Every option is optional: values that are not set in
  // YAML are never written, so the chip keeps its factory / EEPROM defaults.
  void set_direction(uint8_t direction) { this->direction_ = direction; }
  void set_zero_offset(uint16_t position) { this->zero_offset_ = position; }
  void set_hysteresis(uint8_t hysteresis) { this->hysteresis_ = hysteresis; }
  void set_output_mode(uint8_t output_mode) { this->output_mode_ = output_mode; }
  void set_abz_pulses_per_revolution(uint16_t pulses) { this->abz_pulses_per_revolution_ = pulses; }
  void set_z_pulse_width(uint8_t width) { this->z_pulse_width_ = width; }
  void set_uvw_pole_pairs(uint8_t pole_pairs) { this->uvw_pole_pairs_ = pole_pairs; }
  void set_out_pin_mode(uint8_t out_pin_mode) { this->out_pin_mode_ = out_pin_mode; }
  void set_pwm_frequency(uint8_t pwm_frequency) { this->pwm_frequency_ = pwm_frequency; }
  void set_pwm_polarity(uint8_t pwm_polarity) { this->pwm_polarity_ = pwm_polarity; }
  void set_analog_start(uint16_t position) { this->analog_start_ = position; }
  void set_analog_stop(uint16_t position) { this->analog_stop_ = position; }

  /// Persist the current register contents to the chip's EEPROM.
  ///
  /// Non-blocking: pauses sampling, sends the programming sequence and clears
  /// the pause after the required delay. The EEPROM has a limited number of
  /// write cycles, so this must only be triggered by an explicit user action.
  void save_eeprom();

 protected:
  bool read_count(uint16_t &count) override;

  /// Apply all configured options to the chip via read-modify-write.
  void apply_config_();
  /// Read-modify-write the given bits of a register, preserving the others.
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);

  optional<uint8_t> direction_;
  optional<uint16_t> zero_offset_;
  optional<uint8_t> hysteresis_;
  optional<uint8_t> output_mode_;
  optional<uint16_t> abz_pulses_per_revolution_;
  optional<uint8_t> z_pulse_width_;
  optional<uint8_t> uvw_pole_pairs_;
  optional<uint8_t> out_pin_mode_;
  optional<uint8_t> pwm_frequency_;
  optional<uint8_t> pwm_polarity_;
  optional<uint16_t> analog_start_;
  optional<uint16_t> analog_stop_;
};

template<typename... Ts> class SaveEEPROMAction final : public Action<Ts...> {
 public:
  explicit SaveEEPROMAction(MT6701I2CComponent *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->save_eeprom(); }

 protected:
  MT6701I2CComponent *parent_;
};

}  // namespace esphome::mt6701_i2c
