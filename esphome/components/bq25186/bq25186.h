#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_BQ25186_PG_GPO
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/progmem.h"
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
// STAT0 to MASK_ID, read together by every poll: the status for the entities, then the configuration
// registers, which show whether the chip was reset or a write failed
static constexpr size_t BQ25186_STATUS_REGISTER_COUNT = BQ25186_REG_MASK_ID - BQ25186_REG_STAT0 + 1;

// VBAT_CTRL pg_mode: the /PG pin is a general purpose output driven by SYS_REG pg_gpo_level
static constexpr uint8_t BQ25186_VBAT_CTRL_PG_MODE = 0x80;
static constexpr uint8_t BQ25186_SYS_REG_PG_GPO_LEVEL = 0x10;

// Register values for VBAT_CTRL..MASK_ID, packed by code generation
using BQ25186ConfigRegisters = std::array<uint8_t, BQ25186_CONFIG_REGISTER_COUNT>;
using BQ25186StatusRegisters = std::array<uint8_t, BQ25186_STATUS_REGISTER_COUNT>;

#ifdef USE_TEXT_SENSOR
// STAT0[6:5] charge status, then STAT1[4:3] TS status
PROGMEM_STRING_TABLE(BQ25186StatusStrings, "Not Charging", "Constant Current", "Constant Voltage",
                     "Charge Done/Disabled", "Normal", "Hot/Cold (Charging Suspended)", "Cool (Current Reduced)",
                     "Warm (Voltage Reduced)");

/// Publish the two-bit field in the low bits of `value` as string `first + field`, only when it differs from `last`
inline void publish_status_text(text_sensor::TextSensor *sensor, uint8_t &last, uint8_t value, uint8_t first) {
  value &= 0x03;
  if (value == last)
    return;
  last = value;
  sensor->publish_state(BQ25186StatusStrings::get_progmem_str(first + value, BQ25186StatusStrings::LAST_INDEX));
}
#endif

class BQ25186Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_setup_registers(const BQ25186ConfigRegisters &regs) {
    this->regs_ = regs;
    this->configure_on_boot_ = true;
  }
  bool apply_config(const BQ25186ConfigRegisters &regs);
#ifdef USE_BQ25186_STATUS_PUBLISHER
  /// The generated function that publishes this hub's status entities
  HUB_PUBLISHER(status, const BQ25186StatusRegisters &)
#endif
#ifdef USE_BQ25186_PG_GPO
  /// The /PG/GPO pin is put in general purpose output mode when a switch drives it
  SUB_SWITCH(pg_gpo)
  bool write_pg_gpo_level(bool level) {
    return this->update_register_(BQ25186_REG_SYS_REG, BQ25186_SYS_REG_PG_GPO_LEVEL,
                                  level ? BQ25186_SYS_REG_PG_GPO_LEVEL : 0);
  }
#endif
  /// Write the masked bits of SHIP_RST
  bool write_ship_rst(uint8_t mask, uint8_t value) { return this->update_register_(BQ25186_REG_SHIP_RST, mask, value); }

 protected:
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);
  uint8_t expected_register_(size_t index) const;
  bool config_matches_(const BQ25186StatusRegisters &regs) const;
#ifdef USE_BQ25186_PG_GPO
  void check_pg_gpo_(const BQ25186StatusRegisters &status, bool &pending);
#endif

  BQ25186ConfigRegisters regs_{};  // the configuration last applied, written again after a chip reset
  bool configure_on_boot_{false};
  bool configured_{false};  // regs_ went to the chip at least once
};

}  // namespace esphome::bq25186
