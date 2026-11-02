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

class BQ25820Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_bat_sns_res_mohm(float res) { this->bat_sns_res_mohm_ = res; }
  void set_ac_sns_res_mohm(float res) { this->ac_sns_res_mohm_ = res; }
  // Register codes, computed by codegen from the currents and the sense resistors. 0 keeps the reset value.
  void set_charge_current_code(uint16_t code) { this->charge_current_code_ = code; }
  void set_precharge_current_code(uint16_t code) { this->precharge_current_code_ = code; }
  void set_input_current_limit_code(uint16_t code) { this->input_current_limit_code_ = code; }
  void set_input_voltage_limit_code(uint16_t code) { this->input_voltage_limit_code_ = code; }
  void set_reverse_system_voltage_code(uint16_t code) { this->reverse_system_voltage_code_ = code; }
  void set_charge_voltage_code(uint8_t code) { this->charge_voltage_code_ = code; }
  void set_charge_timer(uint8_t code) { this->charge_timer_ = code; }
  void set_topoff_timer(uint8_t code) { this->topoff_timer_ = code; }
  void set_mppt_sweep_timer(uint8_t code) { this->mppt_sweep_timer_ = code; }
  void set_ts_thresholds(uint8_t value) { this->ts_thresholds_ = value; }
  void set_watchdog_enabled(bool enabled) { this->watchdog_enabled_ = enabled; }
  void set_charge_enabled(bool enabled) { this->charge_enabled_ = enabled; }
  void set_hiz_enabled(bool enabled) { this->hiz_enabled_ = enabled; }
  void set_ideal_diode_enabled(bool enabled) { this->ideal_diode_enabled_ = enabled; }
  void set_mppt_enabled(bool enabled) { this->mppt_enabled_ = enabled; }
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
  bool write_config_registers_();
  void publish_status_(const uint8_t *status);
  void publish_adc_();

  float bat_sns_res_mohm_{5.0f};
  float ac_sns_res_mohm_{2.0f};
  uint16_t charge_current_code_{0};
  uint16_t precharge_current_code_{0};
  uint16_t input_current_limit_code_{0};
  uint16_t input_voltage_limit_code_{0};
  uint16_t reverse_system_voltage_code_{0};
  uint8_t charge_voltage_code_{0};
  uint8_t charge_timer_{0};
  uint8_t topoff_timer_{0};
  uint8_t mppt_sweep_timer_{0};
  uint8_t ts_thresholds_{0};
#ifdef USE_TEXT_SENSOR
  uint8_t last_charge_stat_{0xFF};
  uint8_t last_ts_stat_{0xFF};
#endif
  bool watchdog_enabled_{false};
  bool charge_enabled_{true};
  bool hiz_enabled_{false};
  bool ideal_diode_enabled_{false};
  bool mppt_enabled_{false};
  bool force_mppt_sweep_{false};
  bool temp_enabled_{true};
  bool config_written_{false};
};

}  // namespace esphome::bq25820
