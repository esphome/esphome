#pragma once

#include <array>
#include <cmath>

#include "esphome/components/stepper/stepper.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::tmc22xx {

/// A bit field inside one register.
struct RegisterField {
  uint8_t reg;
  uint8_t shift;
  uint8_t width;
  bool is_signed;

  constexpr uint32_t mask() const { return ((width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u)) << shift; }
};

// Registers shared by the TMC22xx family (TMC2202, TMC2208, TMC2209, TMC2224, TMC2225, TMC2226)
static constexpr uint8_t REG_GCONF = 0x00;
static constexpr uint8_t REG_GSTAT = 0x01;
static constexpr uint8_t REG_IOIN = 0x06;
static constexpr uint8_t REG_FACTORY_CONF = 0x07;
static constexpr uint8_t REG_IHOLD_IRUN = 0x10;
static constexpr uint8_t REG_TPOWERDOWN = 0x11;
static constexpr uint8_t REG_TPWMTHRS = 0x13;
static constexpr uint8_t REG_VACTUAL = 0x22;
static constexpr uint8_t REG_CHOPCONF = 0x6C;
static constexpr uint8_t REG_DRV_STATUS = 0x6F;
static constexpr uint8_t REG_PWMCONF = 0x70;
static constexpr uint8_t REG_PWM_SCALE = 0x71;
static constexpr uint8_t REG_PWM_AUTO = 0x72;

// Readable registers that a driver reset sets back to their defaults; the last written values are restored
static constexpr std::array<uint8_t, 4> RESTORED_REGISTERS{REG_GCONF, REG_FACTORY_CONF, REG_CHOPCONF, REG_PWMCONF};

static constexpr RegisterField I_SCALE_ANALOG{REG_GCONF, 0, 1, false};
static constexpr RegisterField INTERNAL_RSENSE{REG_GCONF, 1, 1, false};
static constexpr RegisterField EN_SPREADCYCLE{REG_GCONF, 2, 1, false};
static constexpr RegisterField SHAFT{REG_GCONF, 3, 1, false};
static constexpr RegisterField INDEX_OTPW{REG_GCONF, 4, 1, false};
static constexpr RegisterField INDEX_STEP{REG_GCONF, 5, 1, false};
static constexpr RegisterField PDN_DISABLE{REG_GCONF, 6, 1, false};
static constexpr RegisterField MSTEP_REG_SELECT{REG_GCONF, 7, 1, false};
static constexpr RegisterField MULTISTEP_FILT{REG_GCONF, 8, 1, false};
static constexpr RegisterField VERSION{REG_IOIN, 24, 8, false};
static constexpr RegisterField OTTRIM{REG_FACTORY_CONF, 8, 2, false};
static constexpr RegisterField IHOLD{REG_IHOLD_IRUN, 0, 5, false};
static constexpr RegisterField IRUN{REG_IHOLD_IRUN, 8, 5, false};
static constexpr RegisterField IHOLDDELAY{REG_IHOLD_IRUN, 16, 4, false};
static constexpr RegisterField TPOWERDOWN{REG_TPOWERDOWN, 0, 8, false};
static constexpr RegisterField TPWMTHRS{REG_TPWMTHRS, 0, 20, false};
static constexpr RegisterField VACTUAL{REG_VACTUAL, 0, 24, true};
static constexpr RegisterField TOFF{REG_CHOPCONF, 0, 4, false};
static constexpr RegisterField VSENSE{REG_CHOPCONF, 17, 1, false};
static constexpr RegisterField MRES{REG_CHOPCONF, 24, 4, false};
static constexpr RegisterField INTPOL{REG_CHOPCONF, 28, 1, false};
static constexpr RegisterField DEDGE{REG_CHOPCONF, 29, 1, false};
static constexpr RegisterField CS_ACTUAL{REG_DRV_STATUS, 16, 5, false};
static constexpr RegisterField FREEWHEEL{REG_PWMCONF, 20, 2, false};
static constexpr RegisterField PWM_SCALE_SUM{REG_PWM_SCALE, 0, 8, false};
static constexpr RegisterField PWM_SCALE_AUTO{REG_PWM_SCALE, 16, 9, true};
static constexpr RegisterField PWM_OFS_AUTO{REG_PWM_AUTO, 0, 8, false};
static constexpr RegisterField PWM_GRAD_AUTO{REG_PWM_AUTO, 16, 8, false};

enum StandstillMode : uint8_t {
  STANDSTILL_MODE_NORMAL = 0,
  STANDSTILL_MODE_FREEWHEELING = 1,
  STANDSTILL_MODE_COIL_SHORT_LS = 2,
  STANDSTILL_MODE_COIL_SHORT_HS = 3,
};

/// Driver conditions reported to on_status, in the order of their status bits: GSTAT bits 0-2, then DRV_STATUS bits
/// 0-11.
enum DriverEvent : uint8_t {
  DRIVER_EVENT_RESET = 0,
  DRIVER_EVENT_DRIVER_ERROR,
  DRIVER_EVENT_CHARGE_PUMP_UNDERVOLTAGE,
  DRIVER_EVENT_OVERTEMPERATURE_PREWARNING,
  DRIVER_EVENT_OVERTEMPERATURE,
  DRIVER_EVENT_SHORT_TO_GROUND_A,
  DRIVER_EVENT_SHORT_TO_GROUND_B,
  DRIVER_EVENT_LOW_SIDE_SHORT_A,
  DRIVER_EVENT_LOW_SIDE_SHORT_B,
  DRIVER_EVENT_OPEN_LOAD_A,
  DRIVER_EVENT_OPEN_LOAD_B,
  DRIVER_EVENT_TEMPERATURE_120C,
  DRIVER_EVENT_TEMPERATURE_143C,
  DRIVER_EVENT_TEMPERATURE_150C,
  DRIVER_EVENT_TEMPERATURE_157C,
  DRIVER_EVENT_COUNT,
};

/// Set by a rising edge on DIAG, which signals a driver error or, on chips that support it, a stall.
struct DiagPinStore {
  volatile bool triggered{false};
  static void gpio_intr(DiagPinStore *arg);
};

/// Counts the step pulses the driver reports on INDEX while it runs from VACTUAL.
struct IndexPulseStore {
  volatile int32_t pulses{0};
  volatile int8_t direction{0};
  static void gpio_intr(IndexPulseStore *arg);
};

/// Base class for Trinamic stepper drivers that are configured over the single wire UART interface.
class TMC22XXStepper : public stepper::Stepper, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_address(uint8_t address) { this->address_ = address; }
  void set_enn_pin(GPIOPin *pin) { this->enn_pin_ = pin; }
  void set_step_pin(GPIOPin *pin) { this->step_pin_ = pin; }
  void set_dir_pin(GPIOPin *pin) { this->dir_pin_ = pin; }
  void set_index_pin(InternalGPIOPin *pin) { this->index_pin_ = pin; }
  void set_diag_pin(InternalGPIOPin *pin) { this->diag_pin_ = pin; }
  void set_clock_frequency(uint32_t frequency) { this->clock_frequency_ = frequency; }
  void set_rsense(float rsense) { this->rsense_ = rsense; }
  void set_vsense(bool vsense) { this->vsense_ = vsense; }
  void set_ottrim(uint8_t ottrim) { this->ottrim_ = ottrim; }
  void set_analog_current_scale(bool enable) { this->analog_current_scale_ = enable; }
  void set_initial_run_current(float current) { this->initial_run_current_ = current; }
  void set_initial_hold_current(float current) { this->initial_hold_current_ = current; }
  void set_initial_microsteps(uint16_t microsteps) { this->initial_microsteps_ = microsteps; }

  /// Enable or disable the motor outputs, using the ENN pin when configured and TOFF otherwise.
  void set_enabled(bool enabled);
  bool is_enabled() const { return this->enabled_; }
  /// True while the driver does not answer, until the status poll reads it again (every 10 s).
  bool is_unreachable() const { return this->status_poll_skip_ > 0; }

  /// Called with the condition and whether it became active or cleared.
  template<typename F> void add_on_status_callback(F &&callback) {
    this->status_callback_.add(std::forward<F>(callback));
  }

  void set_microsteps(uint16_t microsteps);
  uint16_t get_microsteps();
  void set_interpolation(bool enable) { this->write_field(INTPOL, enable); }
  void set_spreadcycle(bool enable) { this->write_field(EN_SPREADCYCLE, enable); }
  void set_inverse_direction(bool inverse) { this->write_field(SHAFT, inverse); }
  void set_tpwm_threshold(uint32_t threshold) { this->write_field(TPWMTHRS, threshold); }

  /// Set the motor run or hold current in A RMS.
  /// Not supported with analog current scale, use set_irun() and set_ihold() instead.
  void set_run_current(float current);
  void set_hold_current(float current);
  void set_irun(uint8_t irun) { this->write_field(IRUN, irun); }
  void set_ihold(uint8_t ihold) { this->write_field(IHOLD, ihold); }
  void set_iholddelay(uint8_t delay) { this->write_field(IHOLDDELAY, delay); }
  void set_tpowerdown(uint8_t tpowerdown) { this->write_field(TPOWERDOWN, tpowerdown); }
  void set_standstill_mode(StandstillMode mode) { this->write_field(FREEWHEEL, mode); }
  /// Convert a current scale (0-31) to A RMS, without analog current scale.
  float scale_to_current(uint8_t scale);

  bool write_register(uint8_t reg, uint32_t value);
  optional<uint32_t> read_register(uint8_t reg);
  bool write_field(const RegisterField &field, uint32_t value);
  optional<uint32_t> read_field(const RegisterField &field);
  static uint32_t extract_field(uint32_t data, const RegisterField &field);

 protected:
  /// IC version reported in IOIN by the supported chip.
  virtual uint8_t expected_version() const = 0;
  /// Return the cached value of a write-only register, or nullptr when the register is readable.
  virtual uint32_t *shadow_register(uint8_t reg);
  /// Write the cached write-only registers and the last written configuration registers to the driver, at setup
  /// and after a driver reset.
  virtual void restore_registers();
  /// Write the settings from the configuration to the readable registers.
  void apply_config_();
  /// Called when DIAG rises without a driver error.
  virtual void on_diag_without_error() {}
  /// Read GSTAT and DRV_STATUS and report changes. Returns false when the status could not be read.
  bool update_status_();
  optional<uint8_t> current_to_scale_(float current);
  float full_scale_current_();
  int32_t get_vactual_() const { return static_cast<int32_t>(extract_field(this->vactual_reg_, VACTUAL)); }
  int32_t speed_to_vactual_(float speed) const;
  void add_index_pulses_();
  void loop_serial_();
  void loop_step_dir_();

  GPIOPin *enn_pin_{nullptr};
  GPIOPin *step_pin_{nullptr};
  GPIOPin *dir_pin_{nullptr};
  InternalGPIOPin *index_pin_{nullptr};
  InternalGPIOPin *diag_pin_{nullptr};
  LazyCallbackManager<void(DriverEvent, bool)> status_callback_;
  uint32_t clock_frequency_{12000000};
  // Write-only registers shared by all family members, with their power-on values
  uint32_t ihold_irun_{0x00071703};
  uint32_t tpowerdown_{0x14};
  uint32_t tpwmthrs_{0};
  uint32_t vactual_reg_{0};
  std::array<uint32_t, RESTORED_REGISTERS.size()> restored_values_{};
  // NaN while not configured
  float rsense_{NAN};
  float initial_run_current_{NAN};
  float initial_hold_current_{NAN};
  IndexPulseStore index_store_{};
  uint16_t initial_microsteps_{16};
  uint16_t status_{0};
  optional<bool> vsense_{};
  optional<uint8_t> ottrim_{};
  uint8_t address_{0};
  uint8_t toff_{3};
  uint8_t version_{0};
  uint8_t restored_written_{0};  // Bit per RESTORED_REGISTERS entry that has been written
  uint8_t status_poll_skip_{0};
  int8_t direction_{0};
  DiagPinStore diag_store_{};
  bool analog_current_scale_{false};
  bool enabled_{false};
  bool vsense_active_{false};
  bool step_state_{false};
  bool moving_{false};
  HighFrequencyLoopRequester high_freq_;
};

}  // namespace esphome::tmc22xx
