#pragma once

#include <list>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::vl53l0x {

struct SequenceStepEnables {
  bool tcc, msrc, dss, pre_range, final_range;
};

struct SequenceStepTimeouts {
  uint16_t pre_range_vcsel_period_pclks, final_range_vcsel_period_pclks;

  uint16_t msrc_dss_tcc_mclks, pre_range_mclks, final_range_mclks;
  uint32_t msrc_dss_tcc_us, pre_range_us, final_range_us;
};

enum VcselPeriodType { VCSEL_PERIOD_PRE_RANGE, VCSEL_PERIOD_FINAL_RANGE };

class VL53L0XSensor final : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  VL53L0XSensor();

  void setup() override;

  void dump_config() override;
  void update() override;

  void loop() override;

  void set_signal_rate_limit(float signal_rate_limit) { signal_rate_limit_ = signal_rate_limit; }
  void set_long_range(bool long_range) { long_range_ = long_range; }
  void set_timeout_us(uint32_t timeout_us) { this->timeout_us_ = timeout_us; }
  void set_enable_pin(GPIOPin *enable) { this->enable_pin_ = enable; }
  void set_timing_budget(uint32_t timing_budget) { this->measurement_timing_budget_us_ = timing_budget; }

 protected:
  // The address the sensor powers up with, and returns to after a soft reset or
  // an enable-pin toggle. Re-addressing happens at the end of init.
  static constexpr uint8_t DEFAULT_I2C_ADDRESS = 0x29;

  uint32_t get_measurement_timing_budget_();
  bool set_measurement_timing_budget_(uint32_t budget_us);
  void get_sequence_step_enables_(SequenceStepEnables *enables);
  void get_sequence_step_timeouts_(SequenceStepEnables const *enables, SequenceStepTimeouts *timeouts);
  uint8_t get_vcsel_pulse_period_(VcselPeriodType type);
  uint32_t get_macro_period_(uint8_t vcsel_period_pclks);

  uint32_t timeout_mclks_to_microseconds_(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks);
  uint32_t timeout_microseconds_to_mclks_(uint32_t timeout_period_us, uint8_t vcsel_period_pclks);

  uint16_t decode_timeout_(uint16_t reg_val);
  uint16_t encode_timeout_(uint16_t timeout_mclks);

  bool perform_single_ref_calibration_(uint8_t vhv_init_byte, uint32_t timeout_ms = 1000);
  bool init_sensor_(uint8_t final_address, bool recovery = false);

  float signal_rate_limit_;
  bool long_range_;
  GPIOPin *enable_pin_{nullptr};
  uint32_t measurement_timing_budget_us_{0};
  bool initiated_read_{false};
  bool waiting_for_interrupt_{false};
  uint8_t stop_variable_;
  // Stall detection - when the in-flight measurement started and how long it
  // may take before it is treated as stalled (computed once during init).
  uint32_t measurement_start_ms_{0};
  uint32_t stall_timeout_ms_{0};
  // Self-recovery - consecutive stalled measurements trigger a sensor soft
  // reset + full re-init; recovery_attempts_ counts failed recovery attempts
  // before the component is marked failed.
  static constexpr uint8_t STALLS_BEFORE_RESET = 2;
  static constexpr uint8_t MAX_RECOVERY_ATTEMPTS = 5;
  uint8_t consecutive_stalls_{0};
  uint8_t recovery_attempts_{0};
  // Backoff - while active, the driver stays completely off the I2C bus.
  // Hammering a wedged bus feeds an ESP-IDF i2c_master bug that crashes the
  // whole ESP after "I2C hardware timeout". Start+duration (not "until") so
  // the comparison stays valid across the millis() rollover.
  uint32_t backoff_start_ms_{0};
  uint32_t backoff_duration_ms_{0};
  // Keep the configured address separately; address_ temporarily becomes
  // DEFAULT_I2C_ADDRESS during setup and recovery.
  uint8_t configured_address_{DEFAULT_I2C_ADDRESS};

  uint32_t timeout_us_{};

  static std::list<VL53L0XSensor *> vl53_sensors;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  static bool enable_pin_setup_complete;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

}  // namespace esphome::vl53l0x
