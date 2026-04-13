#include "bq25186.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bq25186 {

static const char *const TAG = "bq25186";

void BQ25186Component::set_config(const BQ25186Config &config) {
  this->config_ = config;

  if (this->config_.vbat_ctrl.battery_regulation_voltage_mv < 3500)
    this->config_.vbat_ctrl.battery_regulation_voltage_mv = 3500;
  if (this->config_.vbat_ctrl.battery_regulation_voltage_mv > 4650)
    this->config_.vbat_ctrl.battery_regulation_voltage_mv = 4650;
  this->config_.vbat_ctrl.pg_mode &= 0x01;

  if (this->config_.ichg_ctrl.charge_current_ma < 5)
    this->config_.ichg_ctrl.charge_current_ma = 5;
  if (this->config_.ichg_ctrl.charge_current_ma > 1000)
    this->config_.ichg_ctrl.charge_current_ma = 1000;

  this->config_.chargectrl0.termination_percent &= 0x03;
  this->config_.chargectrl0.vindpm_mode &= 0x03;
  this->config_.chargectrl0.thermal_regulation &= 0x03;

  this->config_.chargectrl1.battery_ocp_limit &= 0x03;
  this->config_.chargectrl1.battery_uvlo &= 0x07;

  this->config_.ic_ctrl.vlowv_select &= 0x01;
  this->config_.ic_ctrl.recharge_threshold &= 0x01;
  this->config_.ic_ctrl.safety_timer &= 0x03;
  this->config_.ic_ctrl.watchdog &= 0x03;

  this->config_.tmr_ilim.long_press_duration &= 0x03;
  this->config_.tmr_ilim.autowake &= 0x03;
  this->config_.tmr_ilim.input_current_limit &= 0x07;

  this->config_.ship_rst.push_button_long_press_action &= 0x03;
  this->config_.ship_rst.wake1_timer &= 0x01;
  this->config_.ship_rst.wake2_timer &= 0x01;

  this->config_.sys_reg.system_regulation &= 0x07;
  this->config_.sys_reg.sys_mode &= 0x03;

  this->config_.ts_control.ts_hot &= 0x03;
  this->config_.ts_control.ts_cold &= 0x03;
  this->config_.ts_control.ts_ichg &= 0x01;
  this->config_.ts_control.ts_vrcg &= 0x01;
}

bool BQ25186Component::read_all_registers_() {
  if (!this->read_bytes(BQ25186_REG_STAT0, this->data_.registers, 13)) {
    ESP_LOGW(TAG, "Failed to read register bank 0x00-0x0C");
    return false;
  }
  return true;
}

bool BQ25186Component::write_register_(uint8_t reg, uint8_t value) {
  if (!this->write_byte(reg, value)) {
    ESP_LOGW(TAG, "Failed to write register 0x%02X", reg);
    return false;
  }
  return true;
}

bool BQ25186Component::update_register_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t reg_value;
  if (!this->read_byte(reg, &reg_value)) {
    ESP_LOGW(TAG, "Failed to read register 0x%02X", reg);
    return false;
  }
  reg_value = (reg_value & ~mask) | (value & mask);
  return this->write_register_(reg, reg_value);
}

bool BQ25186Component::update_vbat_ctrl_register_() {
  uint16_t millivolts = this->config_.vbat_ctrl.battery_regulation_voltage_mv;
  if (millivolts < 3500)
    millivolts = 3500;
  if (millivolts > 4650)
    millivolts = 4650;

  const uint8_t regulation_code = static_cast<uint8_t>((millivolts - 3500) / 10);
  const uint8_t reg_value = (this->config_.vbat_ctrl.pg_mode << 7) | regulation_code;
  return this->update_register_(BQ25186_REG_VBAT_CTRL, 0xFF, reg_value);
}

bool BQ25186Component::update_ichg_ctrl_register_() {
  uint16_t milliamps = this->config_.ichg_ctrl.charge_current_ma;
  if (milliamps < 5)
    milliamps = 5;
  if (milliamps > 1000)
    milliamps = 1000;

  uint8_t current_code;
  if (milliamps <= 35) {
    current_code = static_cast<uint8_t>(milliamps - 5);
  } else {
    uint16_t rounded = (milliamps / 10) * 10;
    if (rounded < 40)
      rounded = 40;
    current_code = static_cast<uint8_t>(((rounded - 40) / 10) + 31);
    if (current_code > 127)
      current_code = 127;
  }

  const uint8_t value = ((this->config_.ichg_ctrl.charge_enabled ? 0 : 1) << 7) | current_code;
  return this->update_register_(BQ25186_REG_ICHG_CTRL, 0xFF, value);
}

bool BQ25186Component::update_chargectrl0_register_() {
  const auto &chargectrl0_cfg = this->config_.chargectrl0;

  const uint8_t chargectrl0 = (chargectrl0_cfg.flash_charging_mode << 7) | (chargectrl0_cfg.precharge_is_iterm << 6) |
                              (chargectrl0_cfg.termination_percent << 4) | (chargectrl0_cfg.vindpm_mode << 2) |
                              chargectrl0_cfg.thermal_regulation;
  return this->update_register_(BQ25186_REG_CHARGECTRL0, 0xFF, chargectrl0);
}

bool BQ25186Component::update_chargectrl1_register_() {
  const auto &chargectrl1_cfg = this->config_.chargectrl1;
  const uint8_t chargectrl1 = (chargectrl1_cfg.battery_ocp_limit << 6) | (chargectrl1_cfg.battery_uvlo << 3) |
                              (chargectrl1_cfg.mask_charge_status_interrupt << 2) |
                              (chargectrl1_cfg.mask_ilim_interrupt << 1) | chargectrl1_cfg.mask_vindpm_interrupt;
  return this->update_register_(BQ25186_REG_CHARGECTRL1, 0xFF, chargectrl1);
}

bool BQ25186Component::update_ic_ctrl_register_() {
  const auto &ic_ctrl_cfg = this->config_.ic_ctrl;
  const uint8_t ic_ctrl = (ic_ctrl_cfg.ts_auto_function << 7) | (ic_ctrl_cfg.vlowv_select << 6) |
                          (ic_ctrl_cfg.recharge_threshold << 5) | (ic_ctrl_cfg.double_timer_during_dpm << 4) |
                          (ic_ctrl_cfg.safety_timer << 2) | ic_ctrl_cfg.watchdog;
  return this->update_register_(BQ25186_REG_IC_CTRL, 0xFF, ic_ctrl);
}

bool BQ25186Component::update_tmr_ilim_register_() {
  const auto &tmr_ilim_cfg = this->config_.tmr_ilim;
  const uint8_t tmr_ilim = (tmr_ilim_cfg.long_press_duration << 6) | (tmr_ilim_cfg.hw_reset_requires_vin << 5) |
                           (tmr_ilim_cfg.autowake << 3) | tmr_ilim_cfg.input_current_limit;
  return this->update_register_(BQ25186_REG_TMR_ILIM, 0xFF, tmr_ilim);
}

bool BQ25186Component::update_ship_rst_register_() {
  const auto &ship_rst_cfg = this->config_.ship_rst;
  const uint8_t ship_rst = (ship_rst_cfg.push_button_long_press_action << 3) | (ship_rst_cfg.wake1_timer << 2) |
                           (ship_rst_cfg.wake2_timer << 1) | ship_rst_cfg.enable_push_button;
  return this->update_register_(BQ25186_REG_SHIP_RST, 0x1F, ship_rst);
}

bool BQ25186Component::update_sys_reg_register_() {
  const auto &sys_reg_cfg = this->config_.sys_reg;
  const uint8_t value = (sys_reg_cfg.system_regulation << 5) | (sys_reg_cfg.pg_gpo_level << 4) |
                        (sys_reg_cfg.sys_mode << 2) | (sys_reg_cfg.watchdog_15s_enable << 1) |
                        sys_reg_cfg.disable_vdppm;
  return this->update_register_(BQ25186_REG_SYS_REG, 0xFF, value);
}

bool BQ25186Component::update_ts_control_register_() {
  const auto &ts_control_cfg = this->config_.ts_control;
  const uint8_t ts_control = (ts_control_cfg.ts_hot << 6) | (ts_control_cfg.ts_cold << 4) |
                             (ts_control_cfg.ts_warm_disable << 3) | (ts_control_cfg.ts_cool_disable << 2) |
                             (ts_control_cfg.ts_ichg << 1) | ts_control_cfg.ts_vrcg;
  return this->update_register_(BQ25186_REG_TS_CONTROL, 0xFF, ts_control);
}

bool BQ25186Component::update_mask_id_register_() {
  const auto &mask_id_cfg = this->config_.mask_id;
  const uint8_t mask_id = (mask_id_cfg.mask_ts_interrupt << 7) | (mask_id_cfg.mask_treg_interrupt << 6) |
                          (mask_id_cfg.mask_bat_interrupt << 5) | (mask_id_cfg.mask_pg_interrupt << 4);
  return this->update_register_(BQ25186_REG_MASK_ID, 0xF0, mask_id);
}

bool BQ25186Component::apply_configuration_() {
  return (this->update_vbat_ctrl_register_() && this->update_ichg_ctrl_register_() &&
          this->update_chargectrl0_register_() && this->update_chargectrl1_register_() &&
          this->update_ic_ctrl_register_() && this->update_tmr_ilim_register_() && this->update_ship_rst_register_() &&
          this->update_sys_reg_register_() && this->update_ts_control_register_() && this->update_mask_id_register_());
}

bool BQ25186Component::write_pg_gpo_level(bool level) {
  if (this->config_.vbat_ctrl.pg_mode == 0) {
    ESP_LOGW(TAG, "Ignoring PG/GPO switch write because pg_mode is set to power_good");
    return false;
  }

  if (!this->update_register_(BQ25186_REG_SYS_REG, 0x10, level ? 0x10 : 0x00)) {
    ESP_LOGW(TAG, "Failed to update PG/GPO level");
    return false;
  }

  this->config_.sys_reg.pg_gpo_level = level;
  return true;
}

bool BQ25186Component::trigger_software_reset() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x80, 0x80); }

bool BQ25186Component::trigger_shutdown_mode() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x60, 0x20); }

bool BQ25186Component::trigger_ship_mode() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x60, 0x40); }

bool BQ25186Component::trigger_hardware_reset() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x60, 0x60); }

void BQ25186Component::setup() {
  ESP_LOGV(TAG, "Setting up BQ25186...");

  uint8_t mask_id;
  if (!this->read_byte(BQ25186_REG_MASK_ID, &mask_id)) {
    ESP_LOGE(TAG, "Failed to communicate with BQ25186");
    this->mark_failed();
    return;
  }

  const uint8_t device_id = mask_id & 0x0F;
  if (device_id != 0x01) {
    ESP_LOGW(TAG, "Unexpected device id 0x%X (expected 0x1)", device_id);
  }

  if (!this->apply_configuration_()) {
    ESP_LOGE(TAG, "Failed to apply BQ25186 configuration");
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "BQ25186 initialized");
}

void BQ25186Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BQ25186:\n"
                "  Battery Regulation: %u mV\n"
                "  Charge Current: %u mA\n"
                "  Charge Enabled: %s\n"
                "  Input Current Limit Code: %u\n"
                "  SYS Mode: %u\n"
                "  System Regulation: %u",
                this->config_.vbat_ctrl.battery_regulation_voltage_mv, this->config_.ichg_ctrl.charge_current_ma,
                ONOFF(this->config_.ichg_ctrl.charge_enabled), this->config_.tmr_ilim.input_current_limit,
                this->config_.sys_reg.sys_mode, this->config_.sys_reg.system_regulation);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BQ25186 failed");
  }
}

void BQ25186Component::update() {
  if (this->is_failed()) {
    return;
  }

  if (!this->read_all_registers_()) {
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  if (this->pg_gpo_switch_ != nullptr && this->config_.vbat_ctrl.pg_mode == 1) {
    const bool pg_gpo_level = (this->data_.registers[BQ25186_REG_SYS_REG] & 0x10) != 0;
    this->pg_gpo_switch_->publish_state(pg_gpo_level);
  }

  for (auto *listener : this->listeners_) {
    listener->on_data(this->data_);
  }
}

}  // namespace esphome::bq25186
