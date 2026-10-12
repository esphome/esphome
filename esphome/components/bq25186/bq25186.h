#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include <array>

namespace esphome::bq25186 {

static constexpr uint8_t BQ25186_REG_STAT0 = 0x00;
static constexpr uint8_t BQ25186_REG_STAT1 = 0x01;
static constexpr uint8_t BQ25186_REG_FLAG0 = 0x02;
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
// STAT0, STAT1 and FLAG0, read together by every poll
static constexpr size_t BQ25186_STATUS_REGISTER_COUNT = BQ25186_REG_FLAG0 - BQ25186_REG_STAT0 + 1;

// Register values for VBAT_CTRL..MASK_ID, packed by code generation
using BQ25186ConfigRegisters = std::array<uint8_t, BQ25186_CONFIG_REGISTER_COUNT>;
using BQ25186StatusRegisters = std::array<uint8_t, BQ25186_STATUS_REGISTER_COUNT>;

class BQ25186Listener {
 public:
  virtual void on_status(const BQ25186StatusRegisters &status) = 0;
};

class BQ25186Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_setup_registers(const BQ25186ConfigRegisters &regs) {
    this->setup_regs_ = regs;
    this->configure_on_boot_ = true;
  }
  bool apply_config(const BQ25186ConfigRegisters &regs);
#ifdef BQ25186_LISTENER_COUNT
  void register_listener(BQ25186Listener *listener) { this->listeners_.push_back(listener); }
#endif
#ifdef USE_SWITCH
  /// The /PG/GPO pin is switched to general purpose output mode at setup when a switch drives it
  void set_pg_gpo_switch(switch_::Switch *pg_gpo_switch) { this->pg_gpo_switch_ = pg_gpo_switch; }
  bool write_pg_gpo_level(bool level) { return this->update_register_(BQ25186_REG_SYS_REG, 0x10, level ? 0x10 : 0); }
#endif
  /// Set bits of SHIP_RST: the software reset bit, or the ship, shutdown and hardware reset codes
  bool write_ship_rst(uint8_t mask, uint8_t value) { return this->update_register_(BQ25186_REG_SHIP_RST, mask, value); }

 protected:
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);

#ifdef BQ25186_LISTENER_COUNT
  StaticVector<BQ25186Listener *, BQ25186_LISTENER_COUNT> listeners_;
#endif
#ifdef USE_SWITCH
  switch_::Switch *pg_gpo_switch_{nullptr};
#endif
  BQ25186ConfigRegisters setup_regs_{};
  bool configure_on_boot_{false};
};

}  // namespace esphome::bq25186
