#include <cmath>

#include "esphome/core/log.h"

#include "bq25820.h"

namespace esphome::bq25820 {

static const char *const TAG = "bq25820";

BQ25820Component::BQ25820Component() : PollingComponent(500) {}

void BQ25820Component::setup() {
  // Sense voltage resolution is fixed; step = mV/LSB / sense resistor
  this->ibat_step_mA_ = this->bat_sns_res_mOhm_ > 0.0f ? (uint16_t) llround(10.0 / this->bat_sns_res_mOhm_) : 0;
  this->iac_step_mA_ = this->ac_sns_res_mOhm_ > 0.0f ? (uint16_t) llround(4.0 / this->ac_sns_res_mOhm_) : 0;

  uint8_t part_info = 0xFF;
  if (this->read_register(BQ25820_REG_PART_INFORMATION, &part_info) != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "No BQ25820 detected on I2C bus");
    this->mark_failed();
    return;
  }
  // 0x1A = PART_NUM 011b (BQ25820), DEV_REV 010b
  if (part_info != 0x1A) {
    ESP_LOGW(TAG, "Unexpected part information register 0x%02X, expected 0x1A", part_info);
    this->mark_failed();
    return;
  }

  // Reset all registers to default values (bit auto-clears)
  uint8_t power_path = 0x00;
  this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL, &power_path);
  this->write_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL, power_path | 0x80);

  this->write_config_registers();

  if (this->force_mppt_sweep_) {
    // Self-clearing command bit; runs one panel sweep at startup
    uint8_t mppt = this->read_register(BQ25820_REG_MPPT_CONTROL);
    this->write_register(BQ25820_REG_MPPT_CONTROL, (uint8_t) (mppt | 0x80));
  }

  ESP_LOGD(TAG, "BQ25820 initialized");
}

void BQ25820Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ25820");
  ESP_LOGCONFIG(TAG, "  I2C Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Watchdog: %s", this->watchdog_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Charge Voltage (VFB): %u mV", this->charge_voltage_mV_);
  ESP_LOGCONFIG(TAG, "  Input Current Limit: %u mA", this->input_current_limit_mA_);
  ESP_LOGCONFIG(TAG, "  MPPT: %s", this->mppt_enabled_ ? "enabled" : "disabled");
}

// Write the configured registers. Called from setup() after the register
// reset, and on every update() while the watchdog is enabled so watchdog
// expiries cannot silently revert the charge configuration.
void BQ25820Component::write_config_registers() {
  // ICHG_REG: bits 10:2, 50 mA/step, 400-20000 mA
  uint16_t ichg = 0x190;
  if (this->ichg_mA_ != 0) {
    if (this->ichg_mA_ < 400) {
      ESP_LOGD(TAG, "Charge current clamped to 400 mA");
      this->ichg_mA_ = 400;
    } else if (this->ichg_mA_ > 20000) {
      ESP_LOGD(TAG, "Charge current clamped to 20000 mA");
      this->ichg_mA_ = 20000;
    }
    ichg = (uint16_t) ((this->ichg_mA_ / ICHG_REG_STEP_MA) << 2);
    this->write_register_16bit(BQ25820_REG_CHARGE_CURRENT_LIMIT, ichg);
    ESP_LOGI(TAG, "Charge current limit: %u mA", this->ichg_mA_);
  }

  // VFB_REG (0x00): bits 4:0, 2 mV/step, 1504-1566 mV
  uint16_t vfb = this->charge_voltage_mV_;
  if (vfb < VFB_REG_BASE_MV)
    vfb = VFB_REG_BASE_MV;
  if (vfb > 1566)
    vfb = 1566;
  uint8_t vfb_reg = this->read_register(BQ25820_REG_CHARGE_VOLTAGE_LIMIT);
  vfb_reg = (uint8_t) ((vfb_reg & 0xE0) | (((vfb - VFB_REG_BASE_MV) / VFB_REG_STEP_MV) & 0x1F));
  this->write_register(BQ25820_REG_CHARGE_VOLTAGE_LIMIT, vfb_reg);

  // IAC_DPM (0x06): bits 10:2, 125 mA/step, 1-50 A
  uint16_t iin = this->input_current_limit_mA_ / IAC_DPM_STEP_MA;
  if (iin < 8)
    iin = 8;
  if (iin > 400)
    iin = 400;
  this->write_register_16bit(BQ25820_REG_INPUT_CURRENT_LIMIT, iin << 2);

  // VAC_DPM (0x08): bits 13:2, 20 mV/step, 4.4-65 V
  uint16_t vac = this->input_voltage_limit_mV_ / VAC_DPM_STEP_MV;
  if (vac < 220)
    vac = 220;
  if (vac > 3250)
    vac = 3250;
  this->write_register_16bit(BQ25820_REG_INPUT_VOLTAGE_LIMIT, vac << 2);

  // IPRECHG (0x10): bits 9:2, 50 mA/step, 250 mA-10 A
  uint8_t ipre = (uint8_t) (this->precharge_current_mA_ / IPRECHG_STEP_MA);
  if (ipre < 5)
    ipre = 5;
  if (ipre > 200)
    ipre = 200;
  this->write_register(BQ25820_REG_PRECHARGE_CURRENT_LIMIT, ipre << 2);

  // VSYS_REV (0x0C): bits 13:2, 20 mV/step, 3.3-65 V
  uint16_t vrev = this->reverse_system_voltage_mV_ / VSYS_REV_STEP_MV;
  if (vrev < 165)
    vrev = 165;
  if (vrev > 3250)
    vrev = 3250;
  this->write_register_16bit(BQ25820_REG_REVERSE_MODE_VOLTAGE_LIMIT, vrev << 2);

  // Timer control (0x15): TOPOFF[7:6], WATCHDOG[5:4], EN_CHG_TMR, CHG_TMR[2:1], EN_TMR2X
  uint8_t timer = this->read_register(BQ25820_REG_TIMER_CONTROL);
  timer = (uint8_t) ((timer & 0x01) | (this->watchdog_enabled_ ? 0x10 : 0x00) | 0x08 |
                     ((this->charge_timer_code_ & 0x03) << 1) | ((this->topoff_timer_code_ & 0x03) << 6));
  this->write_register(BQ25820_REG_TIMER_CONTROL, timer);

  // Charger control (0x17): EN_HIZ bit 2, EN_CHG bit 0
  uint8_t ctrl = this->read_register(BQ25820_REG_CHARGER_CONTROL);
  ctrl = (uint8_t) ((ctrl & 0x7E) | (this->hiz_enabled_ ? 0x04 : 0x00) | (this->charge_enabled_ ? 0x01 : 0x00));
  this->write_register(BQ25820_REG_CHARGER_CONTROL, ctrl);

  // Power path (0x19): EN_BATFET_IDEAL_DIODE bit 2
  uint8_t pp = this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL);
  pp = (uint8_t) ((pp & 0xFB) | (this->ideal_diode_enabled_ ? 0x04 : 0x00));
  this->write_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL, pp);

  // MPPT control (0x1A): FULL_SWEEP_TMR[2:1], EN_MPPT bit 0
  uint8_t mppt = this->read_register(BQ25820_REG_MPPT_CONTROL);
  mppt = (uint8_t) ((mppt & 0xF8) | ((this->mppt_sweep_timer_code_ & 0x03) << 1) | (this->mppt_enabled_ ? 0x01 : 0x00));
  this->write_register(BQ25820_REG_MPPT_CONTROL, mppt);

  // TS charging thresholds (0x1B): T5[7:6], T3[5:4], T2[3:2], T1[1:0]
  uint8_t tsc = (uint8_t) (((this->ts_t5_ & 0x03) << 6) | ((this->ts_t3_ & 0x03) << 4) | ((this->ts_t2_ & 0x03) << 2) |
                           (this->ts_t1_ & 0x03));
  this->write_register(BQ25820_REG_TS_CHARGING_THRESHOLD_CONTROL, tsc);

  // TS region behavior (0x1C): EN_TS bit 0
  uint8_t ts = this->read_register(BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL);
  ts = (uint8_t) ((ts & 0xFE) | (this->temp_enabled_ ? 0x01 : 0x00));
  this->write_register(BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL, ts);
}

void BQ25820Component::update() {
  BQ25820Data data{};

  // 16-byte multi-read covers the IAC, IBAT, VAC, VBAT, VSYS and TS ADCs
  // (registers 0x2D-0x3A are contiguous)
  uint8_t adc[16] = {};
  if (this->read_register(BQ25820_REG_IAC_ADC, adc, 16) == i2c::ErrorCode::ERROR_OK) {
    data.iac_mA = (int16_t) ((uint16_t) adc[1] << 8 | adc[0]) * this->iac_step_mA_;
    data.ibat_mA = (int16_t) ((uint16_t) adc[3] << 8 | adc[2]) * this->ibat_step_mA_;
    data.vac_mV = (uint16_t) ((uint16_t) adc[5] << 8 | adc[4]) * VAC_ADC_STEP_MV;
    data.vbat_mV = (uint16_t) ((uint16_t) adc[7] << 8 | adc[6]) * VBAT_ADC_STEP_MV;
    data.vsys_mV = (uint16_t) ((uint16_t) adc[9] << 8 | adc[8]) * VSYS_ADC_STEP_MV;
    // 10-bit value, right-aligned across 0x37/0x38, 0.1% units per LSB
    data.ts_percent10 = (uint16_t) (((uint16_t) adc[11] << 8 | adc[10]) * 1000 / 1024);
    // 10-bit VFB reading across 0x39/0x3A, 1 mV per LSB
    data.vfb_mV = (uint16_t) (((uint16_t) adc[13] << 8 | adc[12]) & 0x7FF) * VFB_ADC_STEP_MV;

    // TS pin NTC: 10k to PGND, 10k reference resistor. Beta formula, 10k at 25 C.
    // The ADC reading is a ratio of the internal reference, so no rail voltage is needed.
    if (data.ts_percent10 > 0 && data.ts_percent10 < 1000) {
      float r_ntc = (float) NTC_REF_OHM * data.ts_percent10 / (1000 - data.ts_percent10);
      if (r_ntc < 100.0f)
        r_ntc = 100.0f;
      if (r_ntc > 10000000.0f)
        r_ntc = 10000000.0f;
      data.ts_temp_C = (float) (1.0 / (1.0 / 298.15 + log(r_ntc / NTC_R0_OHM) / NTC_B_CO)) - 273.15f;
    }
  }

  // Charger status registers 0x21-0x26 are contiguous
  uint8_t status[6] = {};
  if (this->read_register(BQ25820_REG_CHARGER_STATUS_1, status, 6) == i2c::ErrorCode::ERROR_OK) {
    data.charge_stat = status[0] & 0x07;
    data.battery_temp_stat = (status[1] >> 4) & 0x07;
    data.pg = (status[1] & 0x80) != 0;
    data.iac_dpm = (status[0] & 0x40) != 0;
    data.vac_dpm = (status[0] & 0x20) != 0;
    data.watchdog = (status[0] & 0x08) != 0;
    data.reverse = (status[2] & 0x04) != 0;
    data.acfet = (status[2] & 0x02) != 0;
    data.batfet = (status[2] & 0x01) != 0;
  }

  uint8_t fault = 0x00;
  if (this->read_register(BQ25820_REG_FAULT_STATUS, &fault) == i2c::ErrorCode::ERROR_OK) {
    data.vac_overvoltage = (fault & 0x40) != 0;
    data.vac_undervoltage = (fault & 0x80) != 0;
    data.ibat_overcurrent = (fault & 0x20) != 0;
    data.vbat_overvoltage = (fault & 0x10) != 0;
    data.thermal_shutdown = (fault & 0x08) != 0;
    data.charge_safety_timer = (fault & 0x04) != 0;
    data.drv_sup_invalid = (fault & 0x02) != 0;
  }

  if (this->watchdog_enabled_ && data.watchdog) {
    ESP_LOGW(TAG, "I2C watchdog expired, re-writing configuration registers");
    this->write_config_registers();
  }

  this->data_ = data;
  for (auto *listener : this->listeners_)
    listener->on_data(this->data_);
}

uint8_t BQ25820Component::read_register(uint8_t reg) {
  uint8_t value = 0;
  this->read_register(reg, &value);
  return value;
}

uint8_t BQ25820Component::read_register(uint8_t reg, uint8_t *out_value) {
  i2c::ErrorCode err = this->I2CDevice::read_register(reg, out_value, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read of 0x%02X failed", reg);
  }
  return err;
}

uint8_t BQ25820Component::read_register(uint8_t reg, uint8_t *out_value, size_t len) {
  i2c::ErrorCode err = this->I2CDevice::read_register(reg, out_value, len);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read of 0x%02X failed", reg);
  }
  return err;
}

void BQ25820Component::write_register(uint8_t reg, uint8_t value) {
  i2c::ErrorCode err = this->I2CDevice::write_register(reg, &value, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write of 0x%02X failed", reg);
  }
}

uint16_t BQ25820Component::read_register_16bit(uint8_t reg) {
  uint8_t value[2] = {};
  this->read_register(reg, value, 2);
  return (uint16_t) value[1] << 8 | value[0];
}

void BQ25820Component::write_register_16bit(uint8_t reg, uint16_t value) {
  // Registers are little-endian: low byte at the lower address, high byte after
  uint8_t data[3] = {(uint8_t) reg, (uint8_t) (value & 0xFF), (uint8_t) (value >> 8)};
  i2c::ErrorCode err = this->write(data, 3);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write of 0x%02X failed", reg);
  }
}

// Charge Voltage Limit (0x00)
uint16_t BQ25820Component::charge_voltage_limit_mV() {
  uint8_t code = this->read_register(BQ25820_REG_CHARGE_VOLTAGE_LIMIT) & 0x1F;
  return (uint16_t) code * VFB_REG_STEP_MV + VFB_REG_BASE_MV;
}

// Charge Current Limit (0x02)
uint16_t BQ25820Component::charge_current_limit_mA() {
  uint16_t code = this->read_register_16bit(BQ25820_REG_CHARGE_CURRENT_LIMIT) >> 2;
  return (uint16_t) code * ICHG_REG_STEP_MA;
}

// Input Current DPM Limit (0x06)
uint16_t BQ25820Component::input_current_regulation_limit_mA() {
  uint16_t code = this->read_register_16bit(BQ25820_REG_INPUT_CURRENT_LIMIT) >> 2;
  return (uint16_t) code * IAC_DPM_STEP_MA;
}

// Input Voltage DPM Limit (0x08)
uint16_t BQ25820Component::input_voltage_regulation_limit_mV() {
  uint16_t code = this->read_register_16bit(BQ25820_REG_INPUT_VOLTAGE_LIMIT) >> 2;
  return (uint16_t) code * VAC_DPM_STEP_MV;
}

// Reverse Mode Input Current Limit (0x0A)
uint16_t BQ25820Component::reverse_mode_current_limit_mA() {
  uint16_t code = this->read_register_16bit(BQ25820_REG_REVERSE_MODE_CURRENT_LIMIT) >> 2;
  return (uint16_t) code * IAC_REV_STEP_MA;
}

// Reverse Mode System Voltage Limit (0x0C)
uint16_t BQ25820Component::reverse_mode_voltage_limit_mv() {
  uint16_t code = this->read_register_16bit(BQ25820_REG_REVERSE_MODE_VOLTAGE_LIMIT) >> 2;
  return (uint16_t) code * VSYS_REV_STEP_MV;
}

// Precharge Current Limit (0x10)
uint16_t BQ25820Component::precharge_current_limit_mA() {
  uint8_t code = this->read_register(BQ25820_REG_PRECHARGE_CURRENT_LIMIT) >> 2;
  return (uint16_t) code * IPRECHG_STEP_MA;
}

// Termination Current Limit (0x12)
uint16_t BQ25820Component::termination_current_limit_mA() {
  uint8_t code = this->read_register(BQ25820_REG_TERMINATION_CURRENT_LIMIT) >> 2;
  return (uint16_t) code * ITERM_STEP_MA;
}
bool BQ25820Component::precharge_termination_enable() {
  return (this->read_register(BQ25820_REG_PRECHARGE_TERMINATION_CONTROL) & 0x8) != 0;
}

uint8_t BQ25820Component::precharge_vbat_percent() {
  return (this->read_register(BQ25820_REG_PRECHARGE_TERMINATION_CONTROL) >> 1) & 0x3;
}

bool BQ25820Component::precharge_trickle_enable() {
  return (this->read_register(BQ25820_REG_PRECHARGE_TERMINATION_CONTROL) & 0x1) != 0;
}

uint8_t BQ25820Component::timer_topoff_minutes() { return (this->read_register(BQ25820_REG_TIMER_CONTROL) >> 6) & 0x3; }

uint8_t BQ25820Component::timer_watchdog_seconds() {
  return (this->read_register(BQ25820_REG_TIMER_CONTROL) >> 4) & 0x3;
}

bool BQ25820Component::timer_charge_enable() { return (this->read_register(BQ25820_REG_TIMER_CONTROL) & 0x8) != 0; }

uint8_t BQ25820Component::timer_charge_time_hrs() {
  return (this->read_register(BQ25820_REG_TIMER_CONTROL) >> 1) & 0x3;
}

bool BQ25820Component::timer_counter_speed() { return (this->read_register(BQ25820_REG_TIMER_CONTROL) & 0x1) != 0; }

uint8_t BQ25820Component::charger_recharge_threshold_percent() {
  return (this->read_register(BQ25820_REG_CHARGER_CONTROL) >> 6) & 0x3;
}

bool BQ25820Component::charger_disable_ce_pin() {
  return (this->read_register(BQ25820_REG_CHARGER_CONTROL) & 0x10) != 0;
}

bool BQ25820Component::charger_bit_reset_behavior() {
  return (this->read_register(BQ25820_REG_CHARGER_CONTROL) & 0x8) != 0;
}

bool BQ25820Component::charger_hiz_enable() { return (this->read_register(BQ25820_REG_CHARGER_CONTROL) & 0x4) != 0; }

bool BQ25820Component::charger_enable_battery_load() {
  return (this->read_register(BQ25820_REG_CHARGER_CONTROL) & 0x2) != 0;
}

bool BQ25820Component::charger_charge_enable() { return (this->read_register(BQ25820_REG_CHARGER_CONTROL) & 0x1) != 0; }

bool BQ25820Component::pin_ichg_enable() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x80) != 0; }

bool BQ25820Component::pin_ilim_hiz_enable() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x40) != 0; }

bool BQ25820Component::pin_pg_disable() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x20) != 0; }

bool BQ25820Component::pin_stats_disable() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x10) != 0; }

bool BQ25820Component::pin_force_stat4_on() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x8) != 0; }

bool BQ25820Component::pin_force_stat3_on() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x4) != 0; }

bool BQ25820Component::pin_force_stat2_on() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x2) != 0; }

bool BQ25820Component::pin_force_stat1_on() { return (this->read_register(BQ25820_REG_PIN_CONTROL) & 0x1) != 0; }

bool BQ25820Component::power_path_reset() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x80) != 0;
}

bool BQ25820Component::power_path_load_enable() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x40) != 0;
}

bool BQ25820Component::power_path_pfm_enable() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x20) != 0;
}

bool BQ25820Component::power_path_force_battery_off() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x10) != 0;
}

bool BQ25820Component::power_path_reduce_drive_voltage() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x8) != 0;
}

bool BQ25820Component::power_path_ideal_diode_enable() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x4) != 0;
}

bool BQ25820Component::power_path_auto_reverse_enable() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x2) != 0;
}

bool BQ25820Component::power_path_reverse_mode_enable() {
  return (this->read_register(BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL) & 0x1) != 0;
}

bool BQ25820Component::mppt_force_sweep() { return (this->read_register(BQ25820_REG_MPPT_CONTROL) & 0x80) != 0; }

uint8_t BQ25820Component::mppt_sweep_timer() { return (this->read_register(BQ25820_REG_MPPT_CONTROL) >> 1) & 0x3; }

bool BQ25820Component::mppt_enable() { return (this->read_register(BQ25820_REG_MPPT_CONTROL) & 0x1) != 0; }

uint8_t BQ25820Component::temp_threshold5_percent() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_THRESHOLD_CONTROL) >> 6) & 0x3;
}

uint8_t BQ25820Component::temp_threshold3_percent() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_THRESHOLD_CONTROL) >> 4) & 0x3;
}

uint8_t BQ25820Component::temp_threshold2_percent() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_THRESHOLD_CONTROL) >> 2) & 0x3;
}

uint8_t BQ25820Component::temp_threshold1_percent() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_THRESHOLD_CONTROL) >> 0) & 0x3;
}

uint8_t BQ25820Component::temp_jeita_warm_percent() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL) >> 5) & 0x3;
}

bool BQ25820Component::temp_jeita_iset_warm() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL) & 0x10) != 0;
}

uint8_t BQ25820Component::temp_jeita_iset_cool() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL) >> 2) & 0x3;
}

bool BQ25820Component::temp_jeita_enable() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL) & 0x2) != 0;
}

bool BQ25820Component::temp_jeita_sensor_pin_enable() {
  return (this->read_register(BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL) & 0x1) != 0;
}

uint8_t BQ25820Component::temp_reverse_hot_percent() {
  return (this->read_register(BQ25820_REG_TS_REVERSE_MODE_THRESHOLD_CONTROL) >> 6) & 0x3;
}

bool BQ25820Component::temp_reverse_cold_threshold() {
  return (this->read_register(BQ25820_REG_TS_REVERSE_MODE_THRESHOLD_CONTROL) & 0x20) != 0;
}

bool BQ25820Component::reverse_sysrev_uv() {
  return (this->read_register(BQ25820_REG_REVERSE_UNDERVOLTAGE_CONTROL) & 0x20) != 0;
}

// VAC Max Power Point Detected (0x1F)
uint16_t BQ25820Component::vac_mpp_mV() {
  uint16_t raw = (uint16_t) this->read_register(BQ25820_REG_VAC_MPP + 1) << 8;
  raw |= this->read_register(BQ25820_REG_VAC_MPP);
  return (uint16_t) ((raw >> 2) * 20);
}

bool BQ25820Component::status_adc_conversion() {
  return (this->read_register(BQ25820_REG_CHARGER_STATUS_1) & 0x80) != 0;
}

bool BQ25820Component::status_iac_dpm() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_1) & 0x40) != 0; }

bool BQ25820Component::status_vac_dpm() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_1) & 0x20) != 0; }

bool BQ25820Component::status_watchdog() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_1) & 0x8) != 0; }

uint8_t BQ25820Component::status_charge() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_1) >> 0) & 0x7; }

bool BQ25820Component::status_powergood() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_2) & 0x80) != 0; }

uint8_t BQ25820Component::status_battery_temp() {
  return (this->read_register(BQ25820_REG_CHARGER_STATUS_2) >> 4) & 0x7;
}

uint8_t BQ25820Component::status_fsync() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_3) >> 4) & 0x3; }

bool BQ25820Component::status_reverse() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_3) & 0x4) != 0; }

bool BQ25820Component::status_acfet() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_3) & 0x2) != 0; }

bool BQ25820Component::status_batfet() { return (this->read_register(BQ25820_REG_CHARGER_STATUS_3) & 0x1) != 0; }

bool BQ25820Component::fault_input_undervolt_protect() {
  return (this->read_register(BQ25820_REG_FAULT_STATUS) & 0x80) != 0;
}

bool BQ25820Component::fault_input_overvolt_protect() {
  return (this->read_register(BQ25820_REG_FAULT_STATUS) & 0x40) != 0;
}

bool BQ25820Component::fault_battery_overcurrent_protect() {
  return (this->read_register(BQ25820_REG_FAULT_STATUS) & 0x20) != 0;
}

bool BQ25820Component::fault_battery_overvolt_protect() {
  return (this->read_register(BQ25820_REG_FAULT_STATUS) & 0x10) != 0;
}

bool BQ25820Component::fault_overtemp_protect() { return (this->read_register(BQ25820_REG_FAULT_STATUS) & 0x8) != 0; }

bool BQ25820Component::fault_charge_safety_timer_expire() {
  return (this->read_register(BQ25820_REG_FAULT_STATUS) & 0x4) != 0;
}

bool BQ25820Component::fault_drv_sup_voltage_invalid() {
  return (this->read_register(BQ25820_REG_FAULT_STATUS) & 0x2) != 0;
}

bool BQ25820Component::status_adc_done() { return (this->read_register(BQ25820_REG_CHARGER_FLAG_1) & 0x80) != 0; }

bool BQ25820Component::status_iac_dpm_active() { return (this->read_register(BQ25820_REG_CHARGER_FLAG_1) & 0x40) != 0; }

bool BQ25820Component::status_vac_dpm_active() { return (this->read_register(BQ25820_REG_CHARGER_FLAG_1) & 0x20) != 0; }

bool BQ25820Component::status_watchdog_stat() { return (this->read_register(BQ25820_REG_CHARGER_FLAG_1) & 0x8) != 0; }

bool BQ25820Component::status_charge_flag() { return (this->read_register(BQ25820_REG_CHARGER_FLAG_1) & 0x1) != 0; }

bool BQ25820Component::status_powergood_toggle_detect() {
  return (this->read_register(BQ25820_REG_CHARGER_FLAG_2) & 0x80) != 0;
}

bool BQ25820Component::status_acfet_toggle_detect() {
  return (this->read_register(BQ25820_REG_CHARGER_FLAG_2) & 0x40) != 0;
}

bool BQ25820Component::status_batfet_toggle_detect() {
  return (this->read_register(BQ25820_REG_CHARGER_FLAG_2) & 0x20) != 0;
}

bool BQ25820Component::status_battery_temp_detect() {
  return (this->read_register(BQ25820_REG_CHARGER_FLAG_2) & 0x10) != 0;
}

bool BQ25820Component::status_reverse_detect() { return (this->read_register(BQ25820_REG_CHARGER_FLAG_2) & 0x8) != 0; }

bool BQ25820Component::status_fsw_sync_detect() { return (this->read_register(BQ25820_REG_CHARGER_FLAG_2) & 0x2) != 0; }

bool BQ25820Component::fault_input_undervolt() { return (this->read_register(BQ25820_REG_FAULT_FLAG) & 0x80) != 0; }

bool BQ25820Component::fault_input_overvolt() { return (this->read_register(BQ25820_REG_FAULT_FLAG) & 0x40) != 0; }

bool BQ25820Component::fault_battery_overcurrent() { return (this->read_register(BQ25820_REG_FAULT_FLAG) & 0x20) != 0; }

bool BQ25820Component::fault_battery_overvolt() { return (this->read_register(BQ25820_REG_FAULT_FLAG) & 0x10) != 0; }

bool BQ25820Component::fault_battery_thermal_shutdown() {
  return (this->read_register(BQ25820_REG_FAULT_FLAG) & 0x8) != 0;
}

bool BQ25820Component::fault_charge_safety_timer() { return (this->read_register(BQ25820_REG_FAULT_FLAG) & 0x4) != 0; }

bool BQ25820Component::fault_drv_sup_voltage() { return (this->read_register(BQ25820_REG_FAULT_FLAG) & 0x2) != 0; }

bool BQ25820Component::charger_mask_adc_done() { return (this->read_register(BQ25820_REG_CHARGER_MASK_1) & 0x80) != 0; }

bool BQ25820Component::charger_mask_iac_dpm() { return (this->read_register(BQ25820_REG_CHARGER_MASK_1) & 0x40) != 0; }

bool BQ25820Component::charger_mask_dpm_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_1) & 0x20) != 0;
}

bool BQ25820Component::charger_mask_watchdog_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_1) & 0x8) != 0;
}

bool BQ25820Component::charger_mask_int_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_1) & 0x1) != 0;
}

bool BQ25820Component::charger_mask_powergood_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_2) & 0x80) != 0;
}

bool BQ25820Component::charger_mask_acfet_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_2) & 0x40) != 0;
}

bool BQ25820Component::charger_mask_batfet_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_2) & 0x20) != 0;
}

bool BQ25820Component::charger_mask_tempsense_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_2) & 0x10) != 0;
}

bool BQ25820Component::charger_mask_reverse_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_2) & 0x8) != 0;
}

bool BQ25820Component::charger_mask_fsw_pulse_disable() {
  return (this->read_register(BQ25820_REG_CHARGER_MASK_2) & 0x2) != 0;
}

bool BQ25820Component::fault_mask_undervolt_pulse_disable() {
  return (this->read_register(BQ25820_REG_FAULT_MASK) & 0x80) != 0;
}

bool BQ25820Component::fault_mask_overvolt_pulse_disable() {
  return (this->read_register(BQ25820_REG_FAULT_MASK) & 0x40) != 0;
}

bool BQ25820Component::fault_mask_batt_overcurrent_pulse_disable() {
  return (this->read_register(BQ25820_REG_FAULT_MASK) & 0x20) != 0;
}

bool BQ25820Component::fault_mask_batt_overvolt_pulse_disable() {
  return (this->read_register(BQ25820_REG_FAULT_MASK) & 0x10) != 0;
}

bool BQ25820Component::fault_mask_thermal_shutdown_pulse_disable() {
  return (this->read_register(BQ25820_REG_FAULT_MASK) & 0x8) != 0;
}

bool BQ25820Component::fault_mask_timer_pulse_disable() {
  return (this->read_register(BQ25820_REG_FAULT_MASK) & 0x4) != 0;
}

bool BQ25820Component::fault_mask_drvsup_pulse_disable() {
  return (this->read_register(BQ25820_REG_FAULT_MASK) & 0x2) != 0;
}

bool BQ25820Component::adc_enable() { return (this->read_register(BQ25820_REG_ADC_CONTROL) & 0x80) != 0; }

bool BQ25820Component::adc_rate() { return (this->read_register(BQ25820_REG_ADC_CONTROL) & 0x40) != 0; }

uint8_t BQ25820Component::adc_sample_rate() { return (this->read_register(BQ25820_REG_ADC_CONTROL) >> 4) & 0x3; }

bool BQ25820Component::adc_average_enable() { return (this->read_register(BQ25820_REG_ADC_CONTROL) & 0x8) != 0; }

bool BQ25820Component::adc_average_init() { return (this->read_register(BQ25820_REG_ADC_CONTROL) & 0x4) != 0; }

bool BQ25820Component::adc_iac_enable() { return (this->read_register(BQ25820_REG_ADC_CHANNEL_CONTROL) & 0x80) == 0; }

bool BQ25820Component::adc_ibat_disable() { return (this->read_register(BQ25820_REG_ADC_CHANNEL_CONTROL) & 0x40) != 0; }

bool BQ25820Component::adc_vac_disable() { return (this->read_register(BQ25820_REG_ADC_CHANNEL_CONTROL) & 0x20) != 0; }

bool BQ25820Component::adc_vbat_disable() { return (this->read_register(BQ25820_REG_ADC_CHANNEL_CONTROL) & 0x10) != 0; }

bool BQ25820Component::adc_vsys_disable() { return (this->read_register(BQ25820_REG_ADC_CHANNEL_CONTROL) & 0x8) != 0; }

bool BQ25820Component::adc_tempsense_disable() {
  return (this->read_register(BQ25820_REG_ADC_CHANNEL_CONTROL) & 0x4) != 0;
}

bool BQ25820Component::adc_vfb_disable() { return (this->read_register(BQ25820_REG_ADC_CHANNEL_CONTROL) & 0x2) != 0; }

int16_t BQ25820Component::adc_iac() {
  int16_t raw = (int16_t) this->read_register_16bit(BQ25820_REG_IAC_ADC);
  return (int16_t) raw * this->iac_step_mA_;
}

int16_t BQ25820Component::adc_ibat() {
  int16_t raw = (int16_t) this->read_register_16bit(BQ25820_REG_IBAT_ADC);
  return (int16_t) raw * this->ibat_step_mA_;
}

uint16_t BQ25820Component::adc_vac() {
  uint16_t raw = this->read_register_16bit(BQ25820_REG_VAC_ADC);
  return (uint16_t) raw * VAC_ADC_STEP_MV;
}

uint16_t BQ25820Component::adc_vbat() {
  uint16_t raw = this->read_register_16bit(BQ25820_REG_VBAT_ADC);
  return (uint16_t) raw * VBAT_ADC_STEP_MV;
}

uint16_t BQ25820Component::adc_vsys() {
  uint16_t raw = this->read_register_16bit(BQ25820_REG_VSYS_ADC);
  return (uint16_t) raw * VSYS_ADC_STEP_MV;
}

// VFB ADC (0x39)
uint16_t BQ25820Component::adc_vfb() {
  uint16_t raw = (uint16_t) this->read_register(BQ25820_REG_VFB_ADC + 1) << 8;
  raw |= this->read_register(BQ25820_REG_VFB_ADC);
  return (uint16_t) raw * VFB_ADC_STEP_MV;
}

uint8_t BQ25820Component::gate_high_strength() {
  return (this->read_register(BQ25820_REG_GATE_DRIVER_STRENGTH_CONTROL) >> 4) & 0x3;
}

uint8_t BQ25820Component::gate_low_strength() {
  return (this->read_register(BQ25820_REG_GATE_DRIVER_STRENGTH_CONTROL) >> 0) & 0x3;
}

uint8_t BQ25820Component::gate_deadtime_ns() {
  return (this->read_register(BQ25820_REG_GATE_DRIVER_DEAD_TIME_CONTROL) >> 0) & 0x3;
}

uint8_t BQ25820Component::part_number() { return (this->read_register(BQ25820_REG_PART_INFORMATION) >> 3) & 0x7; }

uint8_t BQ25820Component::device_revision() { return (this->read_register(BQ25820_REG_PART_INFORMATION) >> 0) & 0x7; }

uint16_t BQ25820Component::reverse_mode_discharge_current_mA() {
  static const uint16_t limits[] = {20000, 15000, 10000, 5000};
  uint8_t code = this->read_register(BQ25820_REG_REVERSE_MODE_BATTERY_DISCHARGE_CURRENT) >> 6;
  return limits[code];
}

bool BQ25820Component::reverse_mode_fast_enable() {
  return (this->read_register(BQ25820_REG_REVERSE_MODE_BATTERY_DISCHARGE_CURRENT) & 0x2) != 0;
}

uint16_t BQ25820Component::get_battery_voltage_mV() {
  return this->read_register_16bit(BQ25820_REG_VBAT_ADC) * VBAT_ADC_STEP_MV;
}

int16_t BQ25820Component::get_charge_current_mA() {
  return (int16_t) this->read_register_16bit(BQ25820_REG_IBAT_ADC) * this->ibat_step_mA_;
}

bool BQ25820Component::is_charging() {
  uint8_t status = this->read_register(BQ25820_REG_CHARGER_STATUS_1);
  uint8_t charge = status & 0x07;
  return charge != CHARGE_STAT_NOT_CHARGING && charge != CHARGE_STAT_DONE;
}

bool BQ25820Component::is_charged() {
  uint8_t status = this->read_register(BQ25820_REG_CHARGER_STATUS_1);
  return (status & 0x07) == CHARGE_STAT_DONE;
}

bool BQ25820Component::is_low_battery() {
  uint8_t status = this->read_register(BQ25820_REG_CHARGER_STATUS_1);
  return (status & 0x07) == CHARGE_STAT_PRECHARGE;
}

void BQ25820Component::set_charge_current_mA(uint16_t current) { this->ichg_mA_ = current; }

void BQ25820Component::set_watchdog_enabled(bool enabled) { this->watchdog_enabled_ = enabled; }

void BQ25820Component::set_bat_sns_res_mOhm(float res) { this->bat_sns_res_mOhm_ = res; }

void BQ25820Component::set_ac_sns_res_mOhm(float res) { this->ac_sns_res_mOhm_ = res; }

void BQ25820Component::set_charge_voltage_mV(uint16_t mv) { this->charge_voltage_mV_ = mv; }

void BQ25820Component::set_input_current_limit_mA(uint16_t ma) { this->input_current_limit_mA_ = ma; }

void BQ25820Component::set_input_voltage_limit_mV(uint16_t mv) { this->input_voltage_limit_mV_ = mv; }

void BQ25820Component::set_precharge_current_mA(uint16_t ma) { this->precharge_current_mA_ = ma; }

void BQ25820Component::set_reverse_system_voltage_mV(uint16_t mv) { this->reverse_system_voltage_mV_ = mv; }

void BQ25820Component::set_charge_enabled(bool enabled) { this->charge_enabled_ = enabled; }

void BQ25820Component::set_hiz_enabled(bool enabled) { this->hiz_enabled_ = enabled; }

void BQ25820Component::set_charge_timer_code(uint8_t code) { this->charge_timer_code_ = code & 0x03; }

void BQ25820Component::set_topoff_timer_code(uint8_t code) { this->topoff_timer_code_ = code & 0x03; }

void BQ25820Component::set_ideal_diode_enabled(bool enabled) { this->ideal_diode_enabled_ = enabled; }

void BQ25820Component::set_mppt_enabled(bool enabled) { this->mppt_enabled_ = enabled; }

void BQ25820Component::set_mppt_sweep_timer_code(uint8_t code) { this->mppt_sweep_timer_code_ = code & 0x03; }

void BQ25820Component::set_force_mppt_sweep(bool enabled) { this->force_mppt_sweep_ = enabled; }

void BQ25820Component::set_temp_enabled(bool enabled) { this->temp_enabled_ = enabled; }

void BQ25820Component::set_ts_thresholds(uint8_t t1, uint8_t t2, uint8_t t3, uint8_t t5) {
  this->ts_t1_ = t1 & 0x03;
  this->ts_t2_ = t2 & 0x03;
  this->ts_t3_ = t3 & 0x03;
  this->ts_t5_ = t5 & 0x03;
}

}  // namespace esphome::bq25820
