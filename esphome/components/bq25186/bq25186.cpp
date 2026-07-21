#include "bq25186.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bq25186 {

static const char *const TAG = "bq25186";

void BQ25186Component::set_setup_config(const BQ25186Config &config) {
  // Avoid storing the config in RAM, just store a callback that will be cleaned up after setup
  BQ25186Config sanitized = config;

  if (sanitized.vbat_ctrl.battery_regulation_voltage_mv < 3500)
    sanitized.vbat_ctrl.battery_regulation_voltage_mv = 3500;
  if (sanitized.vbat_ctrl.battery_regulation_voltage_mv > 4650)
    sanitized.vbat_ctrl.battery_regulation_voltage_mv = 4650;
  sanitized.vbat_ctrl.pg_mode &= 0x01;

  if (sanitized.ichg_ctrl.charge_current_ma < 5)
    sanitized.ichg_ctrl.charge_current_ma = 5;
  if (sanitized.ichg_ctrl.charge_current_ma > 1000)
    sanitized.ichg_ctrl.charge_current_ma = 1000;

  sanitized.chargectrl0.termination_percent &= 0x03;
  sanitized.chargectrl0.vindpm_mode &= 0x03;
  sanitized.chargectrl0.thermal_regulation &= 0x03;

  sanitized.chargectrl1.battery_ocp_limit &= 0x03;
  sanitized.chargectrl1.battery_uvlo &= 0x07;

  sanitized.ic_ctrl.vlowv_select &= 0x01;
  sanitized.ic_ctrl.recharge_threshold &= 0x01;
  sanitized.ic_ctrl.safety_timer &= 0x03;
  sanitized.ic_ctrl.watchdog &= 0x03;

  sanitized.tmr_ilim.long_press_duration &= 0x03;
  sanitized.tmr_ilim.autowake &= 0x03;
  sanitized.tmr_ilim.input_current_limit &= 0x07;

  sanitized.ship_rst.push_button_long_press_action &= 0x03;
  sanitized.ship_rst.wake1_timer &= 0x01;
  sanitized.ship_rst.wake2_timer &= 0x01;

  sanitized.sys_reg.system_regulation &= 0x07;
  sanitized.sys_reg.sys_mode &= 0x03;

  sanitized.ts_control.ts_hot &= 0x03;
  sanitized.ts_control.ts_cold &= 0x03;
  sanitized.ts_control.ts_ichg &= 0x01;
  sanitized.ts_control.ts_vrcg &= 0x01;

  this->setup_config_callback_ = [this, sanitized]() { return this->apply_config(sanitized); };
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

bool BQ25186Component::update_vbat_ctrl_register_(const BQ25186VBatCtrlConfig &config) {
  uint16_t millivolts = config.battery_regulation_voltage_mv;
  if (millivolts < 3500)
    millivolts = 3500;
  if (millivolts > 4650)
    millivolts = 4650;

  const uint8_t regulation_code = static_cast<uint8_t>((millivolts - 3500) / 10);
  const uint8_t value = (config.pg_mode << 7) | regulation_code;
  return this->write_register_(BQ25186_REG_VBAT_CTRL, value);
}

bool BQ25186Component::update_ichg_ctrl_register_(const BQ25186IchgCtrlConfig &config) {
  uint16_t milliamps = config.charge_current_ma;
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

  const uint8_t value = ((config.charge_enabled ? 0 : 1) << 7) | current_code;
  return this->write_register_(BQ25186_REG_ICHG_CTRL, value);
}

bool BQ25186Component::update_chargectrl0_register_(const BQ25186ChargeCtrl0Config &config) {
  const uint8_t chargectrl0 = (config.flash_charging_mode << 7) | (config.precharge_is_iterm << 6) |
                              (config.termination_percent << 4) | (config.vindpm_mode << 2) | config.thermal_regulation;
  return this->write_register_(BQ25186_REG_CHARGECTRL0, chargectrl0);
}

bool BQ25186Component::update_chargectrl1_register_(const BQ25186ChargeCtrl1Config &config) {
  const uint8_t chargectrl1 = (config.battery_ocp_limit << 6) | (config.battery_uvlo << 3) |
                              (config.mask_charge_status_interrupt << 2) | (config.mask_ilim_interrupt << 1) |
                              config.mask_vindpm_interrupt;
  return this->write_register_(BQ25186_REG_CHARGECTRL1, chargectrl1);
}

bool BQ25186Component::update_ic_ctrl_register_(const BQ25186IcCtrlConfig &config) {
  const uint8_t ic_ctrl = (config.ts_auto_function << 7) | (config.vlowv_select << 6) |
                          (config.recharge_threshold << 5) | (config.double_timer_during_dpm << 4) |
                          (config.safety_timer << 2) | config.watchdog;
  return this->write_register_(BQ25186_REG_IC_CTRL, ic_ctrl);
}

bool BQ25186Component::update_tmr_ilim_register_(const BQ25186TmrIlimConfig &config) {
  const uint8_t tmr_ilim = (config.long_press_duration << 6) | (config.hw_reset_requires_vin << 5) |
                           (config.autowake << 3) | config.input_current_limit;
  return this->write_register_(BQ25186_REG_TMR_ILIM, tmr_ilim);
}

bool BQ25186Component::update_ship_rst_register_(const BQ25186ShipRstConfig &config) {
  const uint8_t ship_rst = (config.push_button_long_press_action << 3) | (config.wake1_timer << 2) |
                           (config.wake2_timer << 1) | config.enable_push_button;
  return this->update_register_(BQ25186_REG_SHIP_RST, 0x1F, ship_rst);
}

bool BQ25186Component::update_sys_reg_register_(const BQ25186SysRegConfig &config) {
  const uint8_t value = (config.system_regulation << 5) | (config.pg_gpo_level << 4) | (config.sys_mode << 2) |
                        (config.watchdog_15s_enable << 1) | config.disable_vdppm;
  return this->write_register_(BQ25186_REG_SYS_REG, value);
}

bool BQ25186Component::update_ts_control_register_(const BQ25186TsControlConfig &config) {
  const uint8_t ts_control = (config.ts_hot << 6) | (config.ts_cold << 4) | (config.ts_warm_disable << 3) |
                             (config.ts_cool_disable << 2) | (config.ts_ichg << 1) | config.ts_vrcg;
  return this->write_register_(BQ25186_REG_TS_CONTROL, ts_control);
}

bool BQ25186Component::update_mask_id_register_(const BQ25186MaskIdConfig &config) {
  const uint8_t mask_id = (config.mask_ts_interrupt << 7) | (config.mask_treg_interrupt << 6) |
                          (config.mask_bat_interrupt << 5) | (config.mask_pg_interrupt << 4);
  return this->update_register_(BQ25186_REG_MASK_ID, 0xF0, mask_id);
}

bool BQ25186Component::apply_config(const BQ25186Config &config) {
#ifdef USE_BQ25186_PG_GPO_SWITCH
  // Force pg_mode to 1 (gpo) if the switch is being used
  BQ25186Config effective_config = config;
  if (this->pg_gpo_switch_ != nullptr) {
    effective_config.vbat_ctrl.pg_mode = 1;
  }
#else
  const BQ25186Config &effective_config = config;
#endif

  return (this->update_vbat_ctrl_register_(effective_config.vbat_ctrl) &&
          this->update_ichg_ctrl_register_(effective_config.ichg_ctrl) &&
          this->update_chargectrl0_register_(effective_config.chargectrl0) &&
          this->update_chargectrl1_register_(effective_config.chargectrl1) &&
          this->update_ic_ctrl_register_(effective_config.ic_ctrl) &&
          this->update_tmr_ilim_register_(effective_config.tmr_ilim) &&
          this->update_ship_rst_register_(effective_config.ship_rst) &&
          this->update_sys_reg_register_(effective_config.sys_reg) &&
          this->update_ts_control_register_(effective_config.ts_control) &&
          this->update_mask_id_register_(effective_config.mask_id));
}

#ifdef USE_BQ25186_PG_GPO_SWITCH
bool BQ25186Component::write_pg_gpo_level(bool level) {
  uint8_t vbat_ctrl;
  if (!this->read_byte(BQ25186_REG_VBAT_CTRL, &vbat_ctrl)) {
    ESP_LOGW(TAG, "Failed to read VBAT_CTRL register");
    return false;
  }

  if ((vbat_ctrl & 0x80) == 0) {
    ESP_LOGW(TAG, "Ignoring PG/GPO switch write because pg_mode is set to power_good");
    return false;
  }

  if (!this->update_register_(BQ25186_REG_SYS_REG, 0x10, level ? 0x10 : 0x00)) {
    ESP_LOGW(TAG, "Failed to update PG/GPO level");
    return false;
  }

  return true;
}
#endif

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

  if (this->setup_config_callback_) {
    ESP_LOGD(TAG, "Applying BQ25186 configuration on setup...");
    if (!this->setup_config_callback_()) {
      ESP_LOGE(TAG, "Failed to apply BQ25186 configuration");
      this->mark_failed();
      return;
    }
    this->setup_config_callback_ = nullptr;
  }

#ifdef USE_BQ25186_PG_GPO_SWITCH
  // Initialize the PG/GPO switch state based on the current register values
  if (this->pg_gpo_switch_ != nullptr) {
    if (!this->update_register_(BQ25186_REG_VBAT_CTRL, 0x80, 0x80)) {
      ESP_LOGE(TAG, "Failed to force PG/GPO pin into GPO mode");
      this->mark_failed();
      return;
    }
    if (!this->read_all_registers_()) {
      ESP_LOGE(TAG, "Failed to read BQ25186 registers after setup");
      this->mark_failed();
      return;
    }
    if ((this->data_.registers[BQ25186_REG_VBAT_CTRL] & 0x80) == 0) {
      ESP_LOGW(TAG, "PG/GPO switch will be disabled because pg_mode is set to power_good");
      return;
    }
    this->pg_gpo_switch_->publish_state((this->data_.registers[BQ25186_REG_SYS_REG] & 0x10) != 0);
  }
#endif
  ESP_LOGV(TAG, "BQ25186 initialized");
}

void BQ25186Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ25186:");
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

  for (auto *listener : this->listeners_) {
    listener->on_data(this->data_);
  }
}

}  // namespace esphome::bq25186
