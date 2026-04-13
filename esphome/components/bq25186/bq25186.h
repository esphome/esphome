#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include <vector>

namespace esphome::bq25186 {

static const uint8_t BQ25186_REG_STAT0 = 0x00;
static const uint8_t BQ25186_REG_STAT1 = 0x01;
static const uint8_t BQ25186_REG_FLAG0 = 0x02;
static const uint8_t BQ25186_REG_VBAT_CTRL = 0x03;
static const uint8_t BQ25186_REG_ICHG_CTRL = 0x04;
static const uint8_t BQ25186_REG_CHARGECTRL0 = 0x05;
static const uint8_t BQ25186_REG_CHARGECTRL1 = 0x06;
static const uint8_t BQ25186_REG_IC_CTRL = 0x07;
static const uint8_t BQ25186_REG_TMR_ILIM = 0x08;
static const uint8_t BQ25186_REG_SHIP_RST = 0x09;
static const uint8_t BQ25186_REG_SYS_REG = 0x0A;
static const uint8_t BQ25186_REG_TS_CONTROL = 0x0B;
static const uint8_t BQ25186_REG_MASK_ID = 0x0C;

struct BQ25186Data {
  uint8_t registers[13];
};

class BQ25186Listener {
 public:
  virtual void on_data(const BQ25186Data &data) = 0;
};

class BQ25186Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void add_listener(BQ25186Listener *listener) { this->listeners_.push_back(listener); }

  void set_battery_regulation_voltage(uint16_t millivolts) { this->battery_regulation_voltage_mv_ = millivolts; }
  void set_charge_current(uint16_t milliamps) { this->charge_current_ma_ = milliamps; }
  void set_charge_enabled(bool enabled) { this->charge_enabled_ = enabled; }
  void set_flash_charging_mode(bool enabled) { this->flash_charging_mode_ = enabled; }
  void set_precharge_is_iterm(bool enabled) { this->precharge_is_iterm_ = enabled; }
  void set_termination_percent(uint8_t value) { this->termination_percent_ = value & 0x03; }
  void set_vindpm_mode(uint8_t value) { this->vindpm_mode_ = value & 0x03; }
  void set_thermal_regulation(uint8_t value) { this->thermal_regulation_ = value & 0x03; }
  void set_battery_ocp_limit(uint8_t value) { this->battery_ocp_limit_ = value & 0x03; }
  void set_battery_uvlo(uint8_t value) { this->battery_uvlo_ = value & 0x07; }

  void set_interrupt_masks(bool charge_status, bool ilim, bool vindpm) {
    this->mask_charge_status_interrupt_ = charge_status;
    this->mask_ilim_interrupt_ = ilim;
    this->mask_vindpm_interrupt_ = vindpm;
  }

  void set_ts_auto_function(bool enabled) { this->ts_auto_function_ = enabled; }
  void set_vlowv_select(uint8_t value) { this->vlowv_select_ = value & 0x01; }
  void set_recharge_threshold(uint8_t value) { this->recharge_threshold_ = value & 0x01; }
  void set_double_timer_during_dpm(bool enabled) { this->double_timer_during_dpm_ = enabled; }
  void set_safety_timer(uint8_t value) { this->safety_timer_ = value & 0x03; }
  void set_watchdog(uint8_t value) { this->watchdog_ = value & 0x03; }

  void set_long_press_duration(uint8_t value) { this->long_press_duration_ = value & 0x03; }
  void set_hw_reset_requires_vin(bool enabled) { this->hw_reset_requires_vin_ = enabled; }
  void set_autowake(uint8_t value) { this->autowake_ = value & 0x03; }
  void set_input_current_limit(uint8_t value) { this->input_current_limit_ = value & 0x07; }

  void set_push_button_settings(uint8_t long_press_action, uint8_t wake1, uint8_t wake2, bool enable_push) {
    this->push_button_long_press_action_ = long_press_action & 0x03;
    this->wake1_timer_ = wake1 & 0x01;
    this->wake2_timer_ = wake2 & 0x01;
    this->enable_push_button_ = enable_push;
  }

  void set_system_regulation(uint8_t value) { this->system_regulation_ = value & 0x07; }
  void set_sys_mode(uint8_t value) { this->sys_mode_ = value & 0x03; }
  void set_watchdog_15s_enable(bool enabled) { this->watchdog_15s_enable_ = enabled; }
  void set_disable_vdppm(bool disabled) { this->disable_vdppm_ = disabled; }

  void set_ts_hot(uint8_t value) { this->ts_hot_ = value & 0x03; }
  void set_ts_cold(uint8_t value) { this->ts_cold_ = value & 0x03; }
  void set_ts_warm_disable(bool disabled) { this->ts_warm_disable_ = disabled; }
  void set_ts_cool_disable(bool disabled) { this->ts_cool_disable_ = disabled; }
  void set_ts_ichg(uint8_t value) { this->ts_ichg_ = value & 0x01; }
  void set_ts_vrcg(uint8_t value) { this->ts_vrcg_ = value & 0x01; }

  void set_global_interrupt_masks(bool ts, bool treg, bool bat, bool pg) {
    this->mask_ts_interrupt_ = ts;
    this->mask_treg_interrupt_ = treg;
    this->mask_bat_interrupt_ = bat;
    this->mask_pg_interrupt_ = pg;
  }

  void set_pg_mode(uint8_t value) { this->pg_mode_ = value & 0x01; }
  void set_pg_gpo_switch(switch_::Switch *pg_gpo_switch) { this->pg_gpo_switch_ = pg_gpo_switch; }
  bool write_pg_gpo_level(bool level);
  bool trigger_software_reset();
  bool trigger_shutdown_mode();
  bool trigger_ship_mode();
  bool trigger_hardware_reset();

 protected:
  bool read_all_registers_();
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);
  bool apply_configuration_();

  static uint8_t encode_battery_regulation_voltage(uint16_t millivolts);
  static uint8_t encode_fast_charge_current(uint16_t milliamps);

  BQ25186Data data_{};
  std::vector<BQ25186Listener *> listeners_;

  uint16_t battery_regulation_voltage_mv_{4200};
  uint16_t charge_current_ma_{10};
  bool charge_enabled_{true};
  bool flash_charging_mode_{false};
  bool precharge_is_iterm_{false};
  uint8_t termination_percent_{2};
  uint8_t vindpm_mode_{1};
  uint8_t thermal_regulation_{0};
  uint8_t battery_ocp_limit_{1};
  uint8_t battery_uvlo_{0};
  bool mask_charge_status_interrupt_{true};
  bool mask_ilim_interrupt_{true};
  bool mask_vindpm_interrupt_{false};

  bool ts_auto_function_{true};
  uint8_t vlowv_select_{0};
  uint8_t recharge_threshold_{0};
  bool double_timer_during_dpm_{false};
  uint8_t safety_timer_{1};
  uint8_t watchdog_{0};

  uint8_t long_press_duration_{1};
  bool hw_reset_requires_vin_{false};
  uint8_t autowake_{1};
  uint8_t input_current_limit_{5};

  uint8_t push_button_long_press_action_{2};
  uint8_t wake1_timer_{0};
  uint8_t wake2_timer_{0};
  bool enable_push_button_{true};

  uint8_t system_regulation_{2};
  bool pg_gpo_level_{false};  // true -> pull to low, false -> high impedance
  uint8_t sys_mode_{0};
  bool watchdog_15s_enable_{false};
  bool disable_vdppm_{false};

  uint8_t ts_hot_{0};
  uint8_t ts_cold_{0};
  bool ts_warm_disable_{false};
  bool ts_cool_disable_{false};
  uint8_t ts_ichg_{0};
  uint8_t ts_vrcg_{0};

  bool mask_ts_interrupt_{false};
  bool mask_treg_interrupt_{true};
  bool mask_bat_interrupt_{false};
  bool mask_pg_interrupt_{false};
  uint8_t pg_mode_{0};
  switch_::Switch *pg_gpo_switch_{nullptr};
};

}  // namespace esphome::bq25186
