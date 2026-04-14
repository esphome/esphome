#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include <functional>
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

struct BQ25186VBatCtrlConfig {
  uint16_t battery_regulation_voltage_mv{4200};
  uint8_t pg_mode{0};
};

struct BQ25186IchgCtrlConfig {
  uint16_t charge_current_ma{10};
  bool charge_enabled{true};
};

struct BQ25186ChargeCtrl0Config {
  bool flash_charging_mode{false};
  bool precharge_is_iterm{false};
  uint8_t termination_percent{2};
  uint8_t vindpm_mode{1};
  uint8_t thermal_regulation{0};
};

struct BQ25186ChargeCtrl1Config {
  uint8_t battery_ocp_limit{1};
  uint8_t battery_uvlo{0};
  bool mask_charge_status_interrupt{true};
  bool mask_ilim_interrupt{true};
  bool mask_vindpm_interrupt{false};
};

struct BQ25186IcCtrlConfig {
  bool ts_auto_function{true};
  uint8_t vlowv_select{0};
  uint8_t recharge_threshold{0};
  bool double_timer_during_dpm{false};
  uint8_t safety_timer{1};
  uint8_t watchdog{0};
};

struct BQ25186TmrIlimConfig {
  uint8_t long_press_duration{1};
  bool hw_reset_requires_vin{false};
  uint8_t autowake{1};
  uint8_t input_current_limit{5};
};

struct BQ25186ShipRstConfig {
  uint8_t push_button_long_press_action{2};
  uint8_t wake1_timer{0};
  uint8_t wake2_timer{0};
  bool enable_push_button{true};
};

struct BQ25186SysRegConfig {
  uint8_t system_regulation{2};
  bool pg_gpo_level{false};  // true -> pull to low, false -> high impedance
  uint8_t sys_mode{0};
  bool watchdog_15s_enable{false};
  bool disable_vdppm{false};
};

struct BQ25186TsControlConfig {
  uint8_t ts_hot{0};
  uint8_t ts_cold{0};
  bool ts_warm_disable{false};
  bool ts_cool_disable{false};
  uint8_t ts_ichg{0};
  uint8_t ts_vrcg{0};
};

struct BQ25186MaskIdConfig {
  bool mask_ts_interrupt{false};
  bool mask_treg_interrupt{true};
  bool mask_bat_interrupt{false};
  bool mask_pg_interrupt{false};
};

struct BQ25186Config {
  BQ25186VBatCtrlConfig vbat_ctrl{};
  BQ25186IchgCtrlConfig ichg_ctrl{};
  BQ25186ChargeCtrl0Config chargectrl0{};
  BQ25186ChargeCtrl1Config chargectrl1{};
  BQ25186IcCtrlConfig ic_ctrl{};
  BQ25186TmrIlimConfig tmr_ilim{};
  BQ25186ShipRstConfig ship_rst{};
  BQ25186SysRegConfig sys_reg{};
  BQ25186TsControlConfig ts_control{};
  BQ25186MaskIdConfig mask_id{};
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
  // This will only take effect once per boot, because after
  // that the setup configuration is considered applied and cleared to save RAM.
  void set_setup_config(const BQ25186Config &config);
  bool apply_config(const BQ25186Config &config);

  void add_listener(BQ25186Listener *listener) { this->listeners_.push_back(listener); }

  bool write_pg_gpo_level(bool level);
  bool trigger_software_reset();
  bool trigger_shutdown_mode();
  bool trigger_ship_mode();
  bool trigger_hardware_reset();

 protected:
  using SetupConfigCallback = std::function<bool(void)>;

  bool read_all_registers_();
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);
  bool update_vbat_ctrl_register_(const BQ25186VBatCtrlConfig &config);
  bool update_ichg_ctrl_register_(const BQ25186IchgCtrlConfig &config);
  bool update_chargectrl0_register_(const BQ25186ChargeCtrl0Config &config);
  bool update_chargectrl1_register_(const BQ25186ChargeCtrl1Config &config);
  bool update_ic_ctrl_register_(const BQ25186IcCtrlConfig &config);
  bool update_tmr_ilim_register_(const BQ25186TmrIlimConfig &config);
  bool update_ship_rst_register_(const BQ25186ShipRstConfig &config);
  bool update_sys_reg_register_(const BQ25186SysRegConfig &config);
  bool update_ts_control_register_(const BQ25186TsControlConfig &config);
  bool update_mask_id_register_(const BQ25186MaskIdConfig &config);

  BQ25186Data data_{};
  std::vector<BQ25186Listener *> listeners_;

  SetupConfigCallback setup_config_callback_{};
};

}  // namespace esphome::bq25186
