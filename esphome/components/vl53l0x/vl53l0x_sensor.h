#pragma once

#include <list>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace vl53l0x {

struct SequenceStepEnables {
  bool tcc, msrc, dss, pre_range, final_range;
};

struct SequenceStepTimeouts {
  uint16_t pre_range_vcsel_period_pclks, final_range_vcsel_period_pclks;

  uint16_t msrc_dss_tcc_mclks, pre_range_mclks, final_range_mclks;
  uint32_t msrc_dss_tcc_us, pre_range_us, final_range_us;
};

enum VcselPeriodType { VCSEL_PERIOD_PRE_RANGE, VCSEL_PERIOD_FINAL_RANGE };

class VL53L0XSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  VL53L0XSensor();

  void setup() override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;

  void loop() override;

  void set_signal_rate_limit(float signal_rate_limit) { signal_rate_limit_ = signal_rate_limit; }
  void set_long_range(bool long_range) { long_range_ = long_range; }
  void set_timeout_us(uint32_t timeout_us) { this->timeout_us_ = timeout_us; }
  void set_enable_pin(GPIOPin *enable) { this->enable_pin_ = enable; }

 protected:
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

  bool perform_single_ref_calibration_(uint8_t vhv_init_byte);

  float signal_rate_limit_;
  bool long_range_;
  GPIOPin *enable_pin_{nullptr};
  uint32_t measurement_timing_budget_us_;
  bool initiated_read_{false};
  bool waiting_for_interrupt_{false};
  uint8_t stop_variable_;

  uint16_t timeout_start_us_;
  uint16_t timeout_us_{};

  static std::list<VL53L0XSensor *> vl53_sensors;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  static bool enable_pin_setup_complete;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

}  // namespace vl53l0x
}  // namespace esphome
