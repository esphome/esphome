#pragma once

#include "esphome/components/tmc22xx/tmc22xx.h"

namespace esphome::tmc2209 {

using tmc22xx::RegisterField;

// StallGuard and CoolStep registers, which the TMC2208 does not have
static constexpr uint8_t REG_TCOOLTHRS = 0x14;
static constexpr uint8_t REG_SGTHRS = 0x40;
static constexpr uint8_t REG_SG_RESULT = 0x41;
static constexpr uint8_t REG_COOLCONF = 0x42;

static constexpr RegisterField TCOOLTHRS{REG_TCOOLTHRS, 0, 20, false};
static constexpr RegisterField SGTHRS{REG_SGTHRS, 0, 8, false};
static constexpr RegisterField SG_RESULT{REG_SG_RESULT, 0, 10, false};
static constexpr RegisterField SEMIN{REG_COOLCONF, 0, 4, false};
static constexpr RegisterField SEUP{REG_COOLCONF, 5, 2, false};
static constexpr RegisterField SEMAX{REG_COOLCONF, 8, 4, false};
static constexpr RegisterField SEDN{REG_COOLCONF, 13, 2, false};
static constexpr RegisterField SEIMIN{REG_COOLCONF, 15, 1, false};

class TMC2209Stepper : public tmc22xx::TMC22XXStepper {
 public:
  void dump_config() override;

  /// Called when DIAG reports a stall. StallGuard only works in StealthChop, between TCOOLTHRS and TPWMTHRS.
  template<typename F> void add_on_stall_callback(F &&callback) {
    this->stall_callback_.add(std::forward<F>(callback));
  }

  /// A stall is reported when the StallGuard result drops below twice this value.
  void set_stallguard_threshold(uint8_t threshold) { this->write_field(SGTHRS, threshold); }
  uint8_t get_stallguard_threshold() const { return extract_field(this->sgthrs_, SGTHRS); }
  /// Lower velocity limit, as TSTEP, for CoolStep and the StallGuard output on DIAG.
  void set_tcool_threshold(uint32_t threshold) { this->write_field(TCOOLTHRS, threshold); }
  void set_semin(uint8_t semin) { this->write_field(SEMIN, semin); }
  void set_semax(uint8_t semax) { this->write_field(SEMAX, semax); }
  void set_seup(uint8_t seup) { this->write_field(SEUP, seup); }
  void set_sedn(uint8_t sedn) { this->write_field(SEDN, sedn); }
  void set_seimin(bool seimin) { this->write_field(SEIMIN, seimin); }

 protected:
  uint8_t expected_version() const override { return 0x21; }
  uint32_t *shadow_register(uint8_t reg) override;
  void restore_registers() override;
  void on_diag_without_error() override { this->stall_callback_.call(); }

  LazyCallbackManager<void()> stall_callback_;
  // Write-only registers with their power-on values
  uint32_t tcoolthrs_{0};
  uint32_t sgthrs_{0};
  uint32_t coolconf_{0};
};

}  // namespace esphome::tmc2209
