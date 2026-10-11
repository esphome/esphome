#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

#include <array>

namespace esphome::bq25186 {

static constexpr uint8_t BQ25186_REG_VBAT_CTRL = 0x03;
static constexpr uint8_t BQ25186_REG_ICHG_CTRL = 0x04;
static constexpr uint8_t BQ25186_REG_CHARGECTRL0 = 0x05;
static constexpr uint8_t BQ25186_REG_CHARGECTRL1 = 0x06;
static constexpr uint8_t BQ25186_REG_IC_CTRL = 0x07;
static constexpr uint8_t BQ25186_REG_TMR_ILIM = 0x08;
static constexpr uint8_t BQ25186_REG_SHIP_RST = 0x09;
static constexpr uint8_t BQ25186_REG_SYS_REG = 0x0A;
static constexpr uint8_t BQ25186_REG_TS_CONTROL = 0x0B;
static constexpr uint8_t BQ25186_REG_MASK_ID = 0x0C;
static constexpr size_t BQ25186_CONFIG_REGISTER_COUNT = BQ25186_REG_MASK_ID - BQ25186_REG_VBAT_CTRL + 1;

// Register values for VBAT_CTRL..MASK_ID, packed by code generation
using BQ25186ConfigRegisters = std::array<uint8_t, BQ25186_CONFIG_REGISTER_COUNT>;

class BQ25186Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_setup_registers(const BQ25186ConfigRegisters &regs) {
    this->setup_regs_ = regs;
    this->configure_on_boot_ = true;
  }
  bool apply_config(const BQ25186ConfigRegisters &regs);

 protected:
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);

  BQ25186ConfigRegisters setup_regs_{};
  bool configure_on_boot_{false};
};

}  // namespace esphome::bq25186
