#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::bq25820 {

// WATCHDOG[5:4] in the timer control register
static constexpr uint8_t TIMER_CONTROL_WATCHDOG = 0x30;

class BQ25820Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  // Amps per current ADC step, from the sense resistors
  void set_ibat_scale(float scale) { this->ibat_scale_ = scale; }
  void set_iac_scale(float scale) { this->iac_scale_ = scale; }
  // Register codes and values built by codegen. A current code of 0 keeps the reset value.
  void set_charge_current_code(uint16_t code) { this->charge_current_code_ = code; }
  void set_precharge_current_code(uint16_t code) { this->precharge_current_code_ = code; }
  void set_input_current_limit_code(uint16_t code) { this->input_current_limit_code_ = code; }
  void set_input_voltage_limit_code(uint16_t code) { this->input_voltage_limit_code_ = code; }
  void set_reverse_system_voltage_code(uint16_t code) { this->reverse_system_voltage_code_ = code; }
  void set_charge_voltage_code(uint8_t code) { this->charge_voltage_code_ = code; }
  void set_timer_control(uint8_t value) { this->timer_control_ = value; }
  void set_charger_control(uint8_t value) { this->charger_control_ = value; }
  void set_mppt_control(uint8_t value) { this->mppt_control_ = value; }
  void set_ts_thresholds(uint8_t value) { this->ts_thresholds_ = value; }
  void set_ideal_diode_enabled(bool enabled) { this->ideal_diode_enabled_ = enabled; }
  void set_force_mppt_sweep(bool enabled) { this->force_mppt_sweep_ = enabled; }
  void set_temp_enabled(bool enabled) { this->temp_enabled_ = enabled; }

#ifdef USE_SENSOR
  SUB_SENSOR(vbat)
  SUB_SENSOR(vsys)
  SUB_SENSOR(vac)
  SUB_SENSOR(ibat)
  SUB_SENSOR(iac)
  SUB_SENSOR(temp_in_c)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(charging)
  SUB_BINARY_SENSOR(charged)
  SUB_BINARY_SENSOR(low_battery)
  SUB_BINARY_SENSOR(pg)
  SUB_BINARY_SENSOR(reverse)
  SUB_BINARY_SENSOR(vac_undervoltage)
  SUB_BINARY_SENSOR(vac_overvoltage)
  SUB_BINARY_SENSOR(ibat_overcurrent)
  SUB_BINARY_SENSOR(vbat_overvoltage)
  SUB_BINARY_SENSOR(thermal_shutdown)
  SUB_BINARY_SENSOR(charge_safety_timer)
  SUB_BINARY_SENSOR(drv_supply_invalid)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(charge_status)
  SUB_TEXT_SENSOR(temp_status)
#endif

 protected:
  bool watchdog_enabled_() const { return (this->timer_control_ & TIMER_CONTROL_WATCHDOG) != 0; }
  bool write_config_registers_();
  void publish_status_(const uint8_t *status);
  void publish_adc_();

  float ibat_scale_{0.002f};
  float iac_scale_{0.002f};
  uint16_t charge_current_code_{0};
  uint16_t precharge_current_code_{0};
  uint16_t input_current_limit_code_{0};
  uint16_t input_voltage_limit_code_{0};
  uint16_t reverse_system_voltage_code_{0};
  uint8_t charge_voltage_code_{0};
  uint8_t timer_control_{0};
  uint8_t charger_control_{0};
  uint8_t mppt_control_{0};
  uint8_t ts_thresholds_{0};
#ifdef USE_TEXT_SENSOR
  uint8_t last_charge_stat_{0xFF};
  uint8_t last_ts_stat_{0xFF};
#endif
  bool ideal_diode_enabled_ : 1 {false};
  bool force_mppt_sweep_ : 1 {false};
  bool temp_enabled_ : 1 {true};
  bool config_written_ : 1 {false};
};

}  // namespace esphome::bq25820
