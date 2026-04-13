#include "bq25186.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bq25186 {

static const char *const TAG = "bq25186";

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

uint8_t BQ25186Component::encode_battery_regulation_voltage(uint16_t millivolts) {
  if (millivolts < 3500)
    millivolts = 3500;
  if (millivolts > 4650)
    millivolts = 4650;
  return static_cast<uint8_t>((millivolts - 3500) / 10);
}

uint8_t BQ25186Component::encode_fast_charge_current(uint16_t milliamps) {
  if (milliamps < 5)
    milliamps = 5;
  if (milliamps > 1000)
    milliamps = 1000;

  if (milliamps <= 35) {
    return static_cast<uint8_t>(milliamps - 5);
  }

  uint16_t rounded = (milliamps / 10) * 10;
  if (rounded < 40)
    rounded = 40;
  uint8_t code = static_cast<uint8_t>(((rounded - 40) / 10) + 31);
  if (code > 127)
    code = 127;
  return code;
}

bool BQ25186Component::apply_configuration_() {
  if (!this->update_register_(
          BQ25186_REG_VBAT_CTRL, 0xFF,
          (this->pg_mode_ << 7) | this->encode_battery_regulation_voltage(this->battery_regulation_voltage_mv_))) {
    return false;
  }

  if (!this->update_register_(
          BQ25186_REG_ICHG_CTRL, 0xFF,
          ((this->charge_enabled_ ? 0 : 1) << 7) | this->encode_fast_charge_current(this->charge_current_ma_))) {
    return false;
  }

  const uint8_t chargectrl0 = (this->flash_charging_mode_ << 7) | (this->precharge_is_iterm_ << 6) |
                              (this->termination_percent_ << 4) | (this->vindpm_mode_ << 2) | this->thermal_regulation_;
  if (!this->update_register_(BQ25186_REG_CHARGECTRL0, 0xFF, chargectrl0)) {
    return false;
  }

  const uint8_t chargectrl1 = (this->battery_ocp_limit_ << 6) | (this->battery_uvlo_ << 3) |
                              (this->mask_charge_status_interrupt_ << 2) | (this->mask_ilim_interrupt_ << 1) |
                              this->mask_vindpm_interrupt_;
  if (!this->update_register_(BQ25186_REG_CHARGECTRL1, 0xFF, chargectrl1)) {
    return false;
  }

  const uint8_t ic_ctrl = (this->ts_auto_function_ << 7) | (this->vlowv_select_ << 6) |
                          (this->recharge_threshold_ << 5) | (this->double_timer_during_dpm_ << 4) |
                          (this->safety_timer_ << 2) | this->watchdog_;
  if (!this->update_register_(BQ25186_REG_IC_CTRL, 0xFF, ic_ctrl)) {
    return false;
  }

  const uint8_t tmr_ilim = (this->long_press_duration_ << 6) | (this->hw_reset_requires_vin_ << 5) |
                           (this->autowake_ << 3) | this->input_current_limit_;
  if (!this->update_register_(BQ25186_REG_TMR_ILIM, 0xFF, tmr_ilim)) {
    return false;
  }

  const uint8_t ship_rst = (this->push_button_long_press_action_ << 3) | (this->wake1_timer_ << 2) |
                           (this->wake2_timer_ << 1) | this->enable_push_button_;
  if (!this->update_register_(BQ25186_REG_SHIP_RST, 0x1F, ship_rst)) {
    return false;
  }

  const uint8_t sys_reg = (this->system_regulation_ << 5) | (this->pg_gpo_level_ << 4) | (this->sys_mode_ << 2) |
                          (this->watchdog_15s_enable_ << 1) | this->disable_vdppm_;
  if (!this->update_register_(BQ25186_REG_SYS_REG, 0xFF, sys_reg)) {
    return false;
  }

  const uint8_t ts_control = (this->ts_hot_ << 6) | (this->ts_cold_ << 4) | (this->ts_warm_disable_ << 3) |
                             (this->ts_cool_disable_ << 2) | (this->ts_ichg_ << 1) | this->ts_vrcg_;
  if (!this->update_register_(BQ25186_REG_TS_CONTROL, 0xFF, ts_control)) {
    return false;
  }

  const uint8_t mask_id = (this->mask_ts_interrupt_ << 7) | (this->mask_treg_interrupt_ << 6) |
                          (this->mask_bat_interrupt_ << 5) | (this->mask_pg_interrupt_ << 4);
  if (!this->update_register_(BQ25186_REG_MASK_ID, 0xF0, mask_id)) {
    return false;
  }

  return true;
}

bool BQ25186Component::write_pg_gpo_level(bool level) {
  if (this->pg_mode_ == 0) {
    ESP_LOGW(TAG, "Ignoring PG/GPO switch write because pg_mode is set to power_good");
    return false;
  }

  if (!this->update_register_(BQ25186_REG_SYS_REG, 0x10, level ? 0x10 : 0x00)) {
    ESP_LOGW(TAG, "Failed to update PG/GPO level");
    return false;
  }

  this->pg_gpo_level_ = level;
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
                this->battery_regulation_voltage_mv_, this->charge_current_ma_, ONOFF(this->charge_enabled_),
                this->input_current_limit_, this->sys_mode_, this->system_regulation_);
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

  if (this->pg_gpo_switch_ != nullptr && this->pg_mode_ == 1) {
    const bool pg_gpo_level = (this->data_.registers[BQ25186_REG_SYS_REG] & 0x10) != 0;
    this->pg_gpo_switch_->publish_state(pg_gpo_level);
  }

  for (auto *listener : this->listeners_) {
    listener->on_data(this->data_);
  }
}

}  // namespace esphome::bq25186
