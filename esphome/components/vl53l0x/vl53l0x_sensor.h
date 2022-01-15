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
  uint32_t get_measurement_timing_budget_() {
    SequenceStepEnables enables{};
    SequenceStepTimeouts timeouts{};

    uint16_t start_overhead = 1910;
    uint16_t end_overhead = 960;
    uint16_t msrc_overhead = 660;
    uint16_t tcc_overhead = 590;
    uint16_t dss_overhead = 690;
    uint16_t pre_range_overhead = 660;
    uint16_t final_range_overhead = 550;

    // "Start and end overhead times always present"
    uint32_t budget_us = start_overhead + end_overhead;

    get_sequence_step_enables_(&enables);
    get_sequence_step_timeouts_(&enables, &timeouts);

    if (enables.tcc)
      budget_us += (timeouts.msrc_dss_tcc_us + tcc_overhead);

    if (enables.dss)
      budget_us += 2 * (timeouts.msrc_dss_tcc_us + dss_overhead);
    else if (enables.msrc)
      budget_us += (timeouts.msrc_dss_tcc_us + msrc_overhead);

    if (enables.pre_range)
      budget_us += (timeouts.pre_range_us + pre_range_overhead);

    if (enables.final_range)
      budget_us += (timeouts.final_range_us + final_range_overhead);

    measurement_timing_budget_us_ = budget_us;  // store for internal reuse
    return budget_us;
  }

  bool set_measurement_timing_budget_(uint32_t budget_us) {
    SequenceStepEnables enables{};
    SequenceStepTimeouts timeouts{};

    uint16_t start_overhead = 1320;  // note that this is different than the value in get_
    uint16_t end_overhead = 960;
    uint16_t msrc_overhead = 660;
    uint16_t tcc_overhead = 590;
    uint16_t dss_overhead = 690;
    uint16_t pre_range_overhead = 660;
    uint16_t final_range_overhead = 550;

    uint32_t min_timing_budget = 20000;

    if (budget_us < min_timing_budget) {
      return false;
    }

    uint32_t used_budget_us = start_overhead + end_overhead;

    get_sequence_step_enables_(&enables);
    get_sequence_step_timeouts_(&enables, &timeouts);

    if (enables.tcc) {
      used_budget_us += (timeouts.msrc_dss_tcc_us + tcc_overhead);
    }

    if (enables.dss) {
      used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + dss_overhead);
    } else if (enables.msrc) {
      used_budget_us += (timeouts.msrc_dss_tcc_us + msrc_overhead);
    }

    if (enables.pre_range) {
      used_budget_us += (timeouts.pre_range_us + pre_range_overhead);
    }

    if (enables.final_range) {
      used_budget_us += final_range_overhead;

      // "Note that the final range timeout is determined by the timing
      // budget and the sum of all other timeouts within the sequence.
      // If there is no room for the final range timeout, then an error
      // will be set. Otherwise the remaining time will be applied to
      // the final range."

      if (used_budget_us > budget_us) {
        // "Requested timeout too big."
        return false;
      }

      uint32_t final_range_timeout_us = budget_us - used_budget_us;

      // set_sequence_step_timeout() begin
      // (SequenceStepId == VL53L0X_SEQUENCESTEP_FINAL_RANGE)

      // "For the final range timeout, the pre-range timeout
      //  must be added. To do this both final and pre-range
      //  timeouts must be expressed in macro periods MClks
      //  because they have different vcsel periods."

      uint16_t final_range_timeout_mclks =
          timeout_microseconds_to_mclks_(final_range_timeout_us, timeouts.final_range_vcsel_period_pclks);

      if (enables.pre_range) {
        final_range_timeout_mclks += timeouts.pre_range_mclks;
      }

      write_byte_16(0x71, encode_timeout_(final_range_timeout_mclks));

      // set_sequence_step_timeout() end

      measurement_timing_budget_us_ = budget_us;  // store for internal reuse
    }
    return true;
  }

  void get_sequence_step_enables_(SequenceStepEnables *enables) {
    uint8_t sequence_config = reg(0x01).get();
    enables->tcc = (sequence_config >> 4) & 0x1;
    enables->dss = (sequence_config >> 3) & 0x1;
    enables->msrc = (sequence_config >> 2) & 0x1;
    enables->pre_range = (sequence_config >> 6) & 0x1;
    enables->final_range = (sequence_config >> 7) & 0x1;
  }

  enum VcselPeriodType { VCSEL_PERIOD_PRE_RANGE, VCSEL_PERIOD_FINAL_RANGE };

  void get_sequence_step_timeouts_(SequenceStepEnables const *enables, SequenceStepTimeouts *timeouts) {
    timeouts->pre_range_vcsel_period_pclks = get_vcsel_pulse_period_(VCSEL_PERIOD_PRE_RANGE);

    timeouts->msrc_dss_tcc_mclks = reg(0x46).get() + 1;
    timeouts->msrc_dss_tcc_us =
        timeout_mclks_to_microseconds_(timeouts->msrc_dss_tcc_mclks, timeouts->pre_range_vcsel_period_pclks);

    uint16_t value;
    read_byte_16(0x51, &value);
    timeouts->pre_range_mclks = decode_timeout_(value);
    timeouts->pre_range_us =
        timeout_mclks_to_microseconds_(timeouts->pre_range_mclks, timeouts->pre_range_vcsel_period_pclks);

    timeouts->final_range_vcsel_period_pclks = get_vcsel_pulse_period_(VCSEL_PERIOD_FINAL_RANGE);

    read_byte_16(0x71, &value);
    timeouts->final_range_mclks = decode_timeout_(value);

    if (enables->pre_range) {
      timeouts->final_range_mclks -= timeouts->pre_range_mclks;
    }

    timeouts->final_range_us =
        timeout_mclks_to_microseconds_(timeouts->final_range_mclks, timeouts->final_range_vcsel_period_pclks);
  }

  uint8_t get_vcsel_pulse_period_(VcselPeriodType type) {
    uint8_t vcsel;
    if (type == VCSEL_PERIOD_PRE_RANGE)
      vcsel = reg(0x50).get();
    else if (type == VCSEL_PERIOD_FINAL_RANGE)
      vcsel = reg(0x70).get();
    else
      return 255;

    return (vcsel + 1) << 1;
  }

  uint32_t get_macro_period_(uint8_t vcsel_period_pclks) {
    return ((2304UL * vcsel_period_pclks * 1655UL) + 500UL) / 1000UL;
  }

  uint32_t timeout_mclks_to_microseconds_(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks) {
    uint32_t macro_period_ns = get_macro_period_(vcsel_period_pclks);
    return ((timeout_period_mclks * macro_period_ns) + (macro_period_ns / 2)) / 1000;
  }
  uint32_t timeout_microseconds_to_mclks_(uint32_t timeout_period_us, uint8_t vcsel_period_pclks) {
    uint32_t macro_period_ns = get_macro_period_(vcsel_period_pclks);
    return (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
  }

  uint16_t decode_timeout_(uint16_t reg_val) {
    // format: "(LSByte * 2^MSByte) + 1"
    uint8_t msb = (reg_val >> 8) & 0xFF;
    uint8_t lsb = (reg_val >> 0) & 0xFF;
    return (uint16_t(lsb) << msb) + 1;
  }
  uint16_t encode_timeout_(uint16_t timeout_mclks) {
    // format: "(LSByte * 2^MSByte) + 1"
    uint32_t ls_byte = 0;
    uint16_t ms_byte = 0;

    if (timeout_mclks <= 0)
      return 0;

    ls_byte = timeout_mclks - 1;

    while ((ls_byte & 0xFFFFFF00) > 0) {
      ls_byte >>= 1;
      ms_byte++;
    }

    return (ms_byte << 8) | (ls_byte & 0xFF);
  }

  bool perform_single_ref_calibration_(uint8_t vhv_init_byte) {
    reg(0x00) = 0x01 | vhv_init_byte;  // VL53L0X_REG_SYSRANGE_MODE_START_STOP

    uint32_t start = millis();
    while ((reg(0x13).get() & 0x07) == 0) {
      if (millis() - start > 1000)
        return false;
      yield();
    }

    reg(0x0B) = 0x01;
    reg(0x00) = 0x00;

    return true;
  }

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
