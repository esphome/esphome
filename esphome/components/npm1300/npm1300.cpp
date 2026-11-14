#include "npm1300.h"
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/core/progmem.h"
#endif

namespace esphome::npm1300 {

ESPHOME_LOG_TAG(TAG, "npm1300");

// Registers: the base address of the peripheral in the high byte, the offset in the low byte
static constexpr uint16_t REG_VBUSIN_TASK_UPDATE_ILIM = 0x0200;
static constexpr uint16_t REG_VBUSIN_ILIM0 = 0x0201;
static constexpr uint16_t REG_VBUSIN_ILIM_STARTUP = 0x0202;
static constexpr uint16_t REG_VBUSIN_STATUS = 0x0207;
static constexpr uint16_t REG_BCHG_TASK_RELEASE_ERR = 0x0300;
static constexpr uint16_t REG_BCHG_ENABLE_SET = 0x0304;
static constexpr uint16_t REG_BCHG_ENABLE_CLR = 0x0305;
static constexpr uint16_t REG_BCHG_DISABLE_SET = 0x0306;
static constexpr uint16_t REG_BCHG_DISABLE_CLR = 0x0307;
static constexpr uint16_t REG_BCHG_ISET_MSB = 0x0308;
static constexpr uint16_t REG_BCHG_ISET_DISCHARGE_MSB = 0x030A;
static constexpr uint16_t REG_BCHG_VTERM = 0x030C;
static constexpr uint16_t REG_BCHG_VTERM_R = 0x030D;
static constexpr uint16_t REG_BCHG_VTRICKLE_SEL = 0x030E;
static constexpr uint16_t REG_BCHG_ITERM_SEL = 0x030F;
static constexpr uint16_t REG_BCHG_CHARGE_STATUS = 0x0334;
static constexpr uint16_t REG_BCHG_ERR_REASON = 0x0336;
static constexpr uint16_t REG_BUCK1_ENA_SET = 0x0400;
static constexpr uint16_t REG_BUCK1_NORM_VOUT = 0x0408;
static constexpr uint16_t REG_BUCK_SW_CTRL_SEL = 0x040F;
static constexpr uint16_t REG_BUCK_STATUS = 0x0434;
static constexpr uint16_t REG_ADC_TASK_VBAT = 0x0500;
static constexpr uint16_t REG_ADC_TASK_VBUS = 0x0507;
static constexpr uint16_t REG_ADC_NTC_R_SEL = 0x050A;
static constexpr uint16_t REG_ADC_TASK_AUTO_TIM_UPDATE = 0x050C;
static constexpr uint16_t REG_ADC_RESULTS = 0x0510;
static constexpr uint16_t REG_ADC_IBAT_MEAS_EN = 0x0524;
static constexpr uint16_t REG_TIMER_TARGET_STROBE = 0x0703;
static constexpr uint16_t REG_TIMER_CONFIG = 0x0705;
static constexpr uint16_t REG_TIMER_HI_BYTE = 0x0708;
static constexpr uint16_t REG_LDSW1_TASK_SET = 0x0800;
static constexpr uint16_t REG_LDSW_STATUS = 0x0804;
static constexpr uint16_t REG_LDSW1_LDO_SEL = 0x0808;
static constexpr uint16_t REG_LDSW1_VOUT_SEL = 0x080C;
static constexpr uint16_t REG_SHIP_TASK_HIBERNATE = 0x0B00;
static constexpr uint16_t REG_SHIP_TASK_SHIP_MODE = 0x0B02;

// VBUSINSTATUS
static constexpr uint8_t VBUS_PRESENT = 0x01;
static constexpr uint8_t VBUS_CURRENT_LIMIT_ACTIVE = 0x02;
// BCHGCHARGESTATUS
static constexpr uint8_t CHARGE_STATUS_BATTERY_DETECTED = 0x01;
static constexpr uint8_t CHARGE_STATUS_COMPLETED = 0x02;
static constexpr uint8_t CHARGE_STATUS_TRICKLE = 0x04;
static constexpr uint8_t CHARGE_STATUS_CONSTANT_CURRENT = 0x08;
static constexpr uint8_t CHARGE_STATUS_CONSTANT_VOLTAGE = 0x10;
static constexpr uint8_t CHARGE_STATUS_DIE_TEMP_HIGH = 0x40;
static constexpr uint8_t CHARGE_STATUS_CHARGING =
    CHARGE_STATUS_TRICKLE | CHARGE_STATUS_CONSTANT_CURRENT | CHARGE_STATUS_CONSTANT_VOLTAGE;
// BCHGDISABLESET and BCHGDISABLECLR
static constexpr uint8_t CHARGER_DISABLE_RECHARGE = 0x01;
static constexpr uint8_t CHARGER_DISABLE_NTC = 0x02;
// BUCKSTATUS power good, LDSWSTATUS powered up as load switch or as LDO
static constexpr uint8_t BUCK_POWER_GOOD[2] = {0x04, 0x40};
static constexpr uint8_t LDSW_POWERED[2] = {0x03, 0x0C};
// ADCIBATMEASSTATUS
static constexpr uint8_t IBAT_MEAS_INVALID = 0x10;
static constexpr uint8_t IBAT_MODE_SHIFT = 2;
static constexpr uint8_t IBAT_MODE_CHARGE = 0x03;
// TIMERCONFIG: wake-up timer with the 16 ms prescaler
static constexpr uint8_t TIMER_CONFIG_WAKEUP = 0x04;
static constexpr uint32_t TIMER_TICK_MS = 16;
static constexpr uint32_t TIMER_MAX_TICKS = 0xFFFFFF;

// ADC full scale values (product specification, section ADC)
static constexpr float ADC_MAX_CODE = 1023.0f;
static constexpr float VBAT_FULL_SCALE_V = 5.0f;
static constexpr float VSYS_FULL_SCALE_V = 6.375f;
static constexpr float VBUS_FULL_SCALE_V = 7.5f;
static constexpr float CHARGE_FULL_SCALE_FACTOR = 1.25f;
static constexpr float DISCHARGE_FULL_SCALE_FACTOR = 1.12f;
// BCHGISETDISCHARGE codes of the two discharge limits
static constexpr uint16_t DISCHARGE_LIMIT_200MA_CODE = 84;
// Five conversions of 250 us each; the loop runs the timeout later than this anyway
static constexpr uint32_t CONVERSION_TIME_MS = 2;

static constexpr uint32_t READ_TIMEOUT_ID = 0;

#ifdef USE_TEXT_SENSOR
PROGMEM_STRING_TABLE(ChargerStatusStrings, "Error", "Paused, Die Too Hot", "Charge Complete", "Trickle Charge",
                     "Constant Current", "Constant Voltage", "Discharging", "No Battery", "Not Charging");
#endif

void NPM1300Component::setup() {
  uint8_t status;
  if (!this->setup_check(this->read_reg_(REG_BCHG_CHARGE_STATUS, &status)))
    return;

  // Raise the VBUS current limit before anything turns on a regulator: at the power-on limit of 100 mA the
  // inrush current of a regulator can trip the over-current protection and brown out the board
  if (this->vbus_current_limit_ != NPM1300_UNSET &&
      !this->setup_check(this->write_reg_(REG_VBUSIN_ILIM_STARTUP, this->vbus_current_limit_) &&
                         this->apply_vbus_current_limit_()))
    return;

  if (this->thermistor_ != NPM1300_UNSET && !this->setup_check(this->write_reg_(REG_ADC_NTC_R_SEL, this->thermistor_)))
    return;

  if (this->charger_configured_ && !this->setup_check(this->configure_charger_()))
    return;

  for (uint8_t i = 0; i < NPM1300_OUTPUT_COUNT; i++) {
    if (!this->setup_check(this->configure_output_(i, this->outputs_[i])))
      return;
  }

#ifdef USE_SENSOR
  // The battery current is measured after every battery voltage measurement
  if (!this->setup_check(this->write_reg_(REG_ADC_IBAT_MEAS_EN, 1) && this->read_charge_currents_()))
    return;
#endif

  this->update();
}

bool NPM1300Component::apply_vbus_current_limit_() {
  return this->write_reg_(REG_VBUSIN_ILIM0, this->vbus_current_limit_) &&
         this->write_reg_(REG_VBUSIN_TASK_UPDATE_ILIM, 1);
}

bool NPM1300Component::configure_charger_() {
  const NPM1300ChargerConfig &config = this->charger_;
  // The charge settings are only taken while the charger is off
  if (!this->write_reg_(REG_BCHG_ENABLE_CLR, 1))
    return false;
  const uint8_t current[2] = {static_cast<uint8_t>(config.current >> 1), static_cast<uint8_t>(config.current & 1)};
  const uint8_t discharge[2] = {static_cast<uint8_t>(config.discharge_limit >> 1),
                                static_cast<uint8_t>(config.discharge_limit & 1)};
  uint8_t disable = config.recharge ? 0 : CHARGER_DISABLE_RECHARGE;
  // Without a thermistor the charger reports an NTC error and does not charge
  if (this->thermistor_ == NPM1300_UNSET || this->thermistor_ == 0)
    disable |= CHARGER_DISABLE_NTC;
  const uint8_t enable = (CHARGER_DISABLE_RECHARGE | CHARGER_DISABLE_NTC) & ~disable;
  if (this->write_register16(REG_BCHG_ISET_MSB, current, sizeof(current)) != i2c::ERROR_OK ||
      this->write_register16(REG_BCHG_ISET_DISCHARGE_MSB, discharge, sizeof(discharge)) != i2c::ERROR_OK ||
      !this->write_reg_(REG_BCHG_VTERM, config.termination_voltage) ||
      !this->write_reg_(REG_BCHG_VTERM_R, config.warm_termination_voltage) ||
      !this->write_reg_(REG_BCHG_VTRICKLE_SEL, config.trickle_voltage) ||
      !this->write_reg_(REG_BCHG_ITERM_SEL, config.termination_current) ||
      !this->write_reg_(REG_BCHG_DISABLE_SET, disable) || !this->write_reg_(REG_BCHG_DISABLE_CLR, enable) ||
      // Measure the battery temperature automatically while charging
      !this->write_reg_(REG_ADC_TASK_AUTO_TIM_UPDATE, 1))
    return false;
  return !config.enabled || this->set_charger_enabled(true);
}

bool NPM1300Component::configure_output_(uint8_t index, const NPM1300OutputConfig &config) {
  const bool is_buck = index < 2;
  const uint8_t channel = index & 1;
  if (config.voltage != NPM1300_UNSET) {
    if (is_buck) {
      // The software voltage replaces the one set by the VSET pin only once its SWCTRLSEL bit is set
      if (!this->write_reg_(REG_BUCK1_NORM_VOUT + channel * 2, config.voltage) ||
          !this->update_register16_(REG_BUCK_SW_CTRL_SEL, 1 << channel, 1 << channel))
        return false;
    } else if (!this->write_reg_(REG_LDSW1_VOUT_SEL + channel, config.voltage)) {
      return false;
    }
  }
  if (!is_buck && config.ldo_mode != NPM1300_UNSET && !this->write_reg_(REG_LDSW1_LDO_SEL + channel, config.ldo_mode))
    return false;
  if (config.enabled != NPM1300_UNSET)
    return this->set_output_enabled(static_cast<NPM1300Output>(index), config.enabled != 0);
  return true;
}

bool NPM1300Component::update_register16_(uint16_t reg, uint8_t mask, uint8_t value) {
  uint8_t current;
  if (!this->read_reg_(reg, &current))
    return false;
  const uint8_t updated = (current & ~mask) | (value & mask);
  return updated == current || this->write_reg_(reg, updated);
}

bool NPM1300Component::read_charge_currents_() {
  uint8_t charge[2];
  uint8_t discharge[2];
  if (!this->read_reg_(REG_BCHG_ISET_MSB, charge, sizeof(charge)) ||
      !this->read_reg_(REG_BCHG_ISET_DISCHARGE_MSB, discharge, sizeof(discharge)))
    return false;
  // The charge current is 2 mA per step of the 9-bit code; the discharge limit has two settings
  const uint16_t charge_ma = ((charge[0] << 1) | (charge[1] & 1)) * 2;
  const uint16_t discharge_code = (discharge[0] << 1) | (discharge[1] & 1);
  const uint16_t discharge_ma = discharge_code <= DISCHARGE_LIMIT_200MA_CODE ? 200 : 1000;
  this->charge_full_scale_ma_ = charge_ma * CHARGE_FULL_SCALE_FACTOR;
  this->discharge_full_scale_ma_ = discharge_ma * DISCHARGE_FULL_SCALE_FACTOR;
  return true;
}

bool NPM1300Component::set_charger_enabled(bool enabled) {
  if (!enabled)
    return this->write_reg_(REG_BCHG_ENABLE_CLR, 1);
  // A charger error stops the charger until it is released
  return this->write_reg_(REG_BCHG_TASK_RELEASE_ERR, 1) && this->write_reg_(REG_BCHG_ENABLE_SET, 1);
}

bool NPM1300Component::set_output_enabled(NPM1300Output output, bool enabled) {
  const uint8_t index = static_cast<uint8_t>(output);
  // A pair of SET and CLR task registers per output
  const uint16_t base = index < 2 ? REG_BUCK1_ENA_SET : REG_LDSW1_TASK_SET;
  const uint16_t reg = base + (index & 1) * 2 + (enabled ? 0 : 1);
  if (!this->write_reg_(reg, 1)) {
    ESP_LOGW(TAG, "Turning output %u %s failed", index, enabled ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"));
    return false;
  }
  return true;
}

void NPM1300Component::enter_ship_mode() {
  ESP_LOGI(TAG, "Entering ship mode");
  this->write_reg_(REG_SHIP_TASK_SHIP_MODE, 1);
}

void NPM1300Component::hibernate(uint32_t duration_ms) {
  const uint32_t ticks = std::min(duration_ms / TIMER_TICK_MS, TIMER_MAX_TICKS);
  uint8_t timer[3];
  put_be24(timer, ticks);
  ESP_LOGI(TAG, "Hibernating for %" PRIu32 " ms", ticks * TIMER_TICK_MS);
  if (!this->write_reg_(REG_TIMER_CONFIG, TIMER_CONFIG_WAKEUP) ||
      this->write_register16(REG_TIMER_HI_BYTE, timer, sizeof(timer)) != i2c::ERROR_OK ||
      !this->write_reg_(REG_TIMER_TARGET_STROBE, 1)) {
    ESP_LOGW(TAG, "Setting the wake-up timer failed");
    return;
  }
  // The timer needs a moment to load the new period
  delay(1);
  this->write_reg_(REG_SHIP_TASK_HIBERNATE, 1);
}

void NPM1300Component::update() {
  uint8_t vbus_status;
  uint8_t charge_status;
  uint8_t error_reason;
  if (!this->status_comm_check(this->read_reg_(REG_VBUSIN_STATUS, &vbus_status) &&
                               this->read_reg_(REG_BCHG_CHARGE_STATUS, &charge_status) &&
                               this->read_reg_(REG_BCHG_ERR_REASON, &error_reason)))
    return;

  // The chip goes back to its default VBUS current limit whenever VBUS is removed
  if ((vbus_status & VBUS_PRESENT) != 0 && this->vbus_current_limit_ != NPM1300_UNSET &&
      !this->status_comm_check(this->apply_vbus_current_limit_()))
    return;

  this->publish_status_(vbus_status, charge_status, error_reason);
#ifdef USE_SWITCH
  this->publish_switches_();
#endif

#ifdef USE_SENSOR
  if (this->vbus_current_limit_sensor_ != nullptr) {
    uint8_t code;
    if (!this->status_comm_check(this->read_reg_(REG_VBUSIN_ILIM0, &code)))
      return;
    // Code 0 (the reset value) is 500 mA, codes 1 to 4 are 100 mA, then 100 mA per step of the code
    const uint16_t milliamps = code == 0 ? 500 : (code < 5 ? 100 : code * 100);
    this->vbus_current_limit_sensor_->publish_state(milliamps / 1000.0f);
  }
  // Start the battery voltage (followed by the battery current), NTC, die temperature and system voltage
  // conversions, then the VBUS one
  static constexpr uint8_t START[4] = {1, 1, 1, 1};
  if (!this->status_comm_check(this->write_register16(REG_ADC_TASK_VBAT, START, sizeof(START)) == i2c::ERROR_OK &&
                               this->write_reg_(REG_ADC_TASK_VBUS, 1)))
    return;
  this->set_timeout(READ_TIMEOUT_ID, CONVERSION_TIME_MS, [this]() { this->read_measurements_(); });
#else
  this->status_comm_result(true);
#endif
}

void NPM1300Component::publish_status_(uint8_t vbus_status, uint8_t charge_status, uint8_t error_reason) {
  const bool vbus_present = (vbus_status & VBUS_PRESENT) != 0;
  if (error_reason != this->last_error_reason_) {
    this->last_error_reason_ = error_reason;
    if (error_reason != 0) {
      ESP_LOGW(TAG, "Charger error, BCHGERRREASON 0x%02X", error_reason);
    }
  }
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->vbus_present_binary_sensor_, vbus_present);
  PUBLISH_STATE(this->vbus_current_limit_active_binary_sensor_, (vbus_status & VBUS_CURRENT_LIMIT_ACTIVE) != 0);
  PUBLISH_STATE(this->charging_binary_sensor_, (charge_status & CHARGE_STATUS_CHARGING) != 0);
  PUBLISH_STATE(this->charge_complete_binary_sensor_, (charge_status & CHARGE_STATUS_COMPLETED) != 0);
  PUBLISH_STATE(this->battery_detected_binary_sensor_, (charge_status & CHARGE_STATUS_BATTERY_DETECTED) != 0);
  PUBLISH_STATE(this->charger_error_binary_sensor_, error_reason != 0);
#endif
#ifdef USE_TEXT_SENSOR
  if (this->charger_status_text_sensor_ != nullptr) {
    uint8_t index;
    if (error_reason != 0) {
      index = 0;
    } else if (charge_status & CHARGE_STATUS_DIE_TEMP_HIGH) {
      index = 1;
    } else if (charge_status & CHARGE_STATUS_COMPLETED) {
      index = 2;
    } else if (charge_status & CHARGE_STATUS_TRICKLE) {
      index = 3;
    } else if (charge_status & CHARGE_STATUS_CONSTANT_CURRENT) {
      index = 4;
    } else if (charge_status & CHARGE_STATUS_CONSTANT_VOLTAGE) {
      index = 5;
    } else if (!vbus_present) {
      index = 6;
    } else if (!(charge_status & CHARGE_STATUS_BATTERY_DETECTED)) {
      index = 7;
    } else {
      index = 8;
    }
    this->charger_status_text_sensor_->publish_state(
        ChargerStatusStrings::get_progmem_str(index, ChargerStatusStrings::LAST_INDEX));
  }
#endif
}

#ifdef USE_SWITCH
void NPM1300Component::publish_switches_() {
  uint8_t value;
  if (this->charger_switch_ != nullptr && this->read_reg_(REG_BCHG_ENABLE_SET, &value))
    this->charger_switch_->publish_state((value & 1) != 0);
  switch_::Switch *const bucks[2] = {this->buck1_switch_, this->buck2_switch_};
  if ((bucks[0] != nullptr || bucks[1] != nullptr) && this->read_reg_(REG_BUCK_STATUS, &value)) {
    for (uint8_t i = 0; i < 2; i++)
      PUBLISH_STATE(bucks[i], (value & BUCK_POWER_GOOD[i]) != 0);
  }
  switch_::Switch *const ldos[2] = {this->ldo1_switch_, this->ldo2_switch_};
  if ((ldos[0] != nullptr || ldos[1] != nullptr) && this->read_reg_(REG_LDSW_STATUS, &value)) {
    for (uint8_t i = 0; i < 2; i++)
      PUBLISH_STATE(ldos[i], (value & LDSW_POWERED[i]) != 0);
  }
}
#endif

#ifdef USE_SENSOR
// A 10-bit result: the high 8 bits in their own register, the low 2 bits in a shared one
static uint16_t adc_code(uint8_t msb, uint8_t lsbs, uint8_t shift) { return (msb << 2) | ((lsbs >> shift) & 0x03); }

void NPM1300Component::read_measurements_() {
  // ADCIBATMEASSTATUS, then the VBAT, NTC, TEMP and VSYS results, their low bits, VBAT0 to VBAT3 (VBAT2 holds the
  // battery current and VBAT3 the VBUS voltage) and their low bits
  uint8_t results[11];
  if (!this->status_comm_result(this->read_reg_(REG_ADC_RESULTS, results, sizeof(results))))
    return;
  const uint8_t ibat_status = results[0];
  const uint8_t lsbs_a = results[5];
  const uint8_t lsbs_b = results[10];

  PUBLISH_STATE(this->battery_voltage_sensor_, adc_code(results[1], lsbs_a, 0) * VBAT_FULL_SCALE_V / ADC_MAX_CODE);
  PUBLISH_STATE(this->system_voltage_sensor_, adc_code(results[4], lsbs_a, 6) * VSYS_FULL_SCALE_V / ADC_MAX_CODE);
  PUBLISH_STATE(this->vbus_voltage_sensor_, adc_code(results[9], lsbs_b, 6) * VBUS_FULL_SCALE_V / ADC_MAX_CODE);
  // Product specification: T = 394.67 - 0.7926 * code
  PUBLISH_STATE(this->die_temperature_sensor_, 394.67f - 0.7926f * adc_code(results[3], lsbs_a, 4));

  if (this->battery_temperature_sensor_ != nullptr) {
    // The NTC divider gives code = 1024 * R / (R + R_nominal)
    const uint16_t code = adc_code(results[2], lsbs_a, 2);
    float temperature = NAN;
    if (code > 0 && code < 1024 && this->thermistor_beta_ != 0) {
      const float inv_kelvin = 1.0f / 298.15f - logf(1024.0f / code - 1.0f) / this->thermistor_beta_;
      temperature = 1.0f / inv_kelvin - 273.15f;
    }
    this->battery_temperature_sensor_->publish_state(temperature);
  }

  if (this->battery_current_sensor_ != nullptr) {
    const uint16_t code = adc_code(results[8], lsbs_b, 4);
    const uint8_t mode = (ibat_status >> IBAT_MODE_SHIFT) & 0x03;
    float current = NAN;
    if ((ibat_status & IBAT_MEAS_INVALID) == 0 && mode != 0) {
      // Positive while charging, negative while discharging
      current = mode == IBAT_MODE_CHARGE ? code * this->charge_full_scale_ma_ : -code * this->discharge_full_scale_ma_;
      current /= ADC_MAX_CODE * 1000.0f;
    }
    this->battery_current_sensor_->publish_state(current);
  }
}
#endif

void NPM1300Component::dump_config() {
  ESP_LOGCONFIG(TAG, "nPM1300:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->vbus_current_limit_ != NPM1300_UNSET) {
    // 100 mA per step of the code
    ESP_LOGCONFIG(TAG, "  VBUS current limit: %u mA", this->vbus_current_limit_ * 100u);
  }
  if (this->charger_configured_) {
    const NPM1300ChargerConfig &config = this->charger_;
    ESP_LOGCONFIG(TAG,
                  "  Charger:\n"
                  "    Current: %u mA\n"
                  "    Discharge limit: %u mA\n"
                  "    Enabled: %s\n"
                  "    Recharge: %s",
                  config.current * 2u, config.discharge_limit <= DISCHARGE_LIMIT_200MA_CODE ? 200u : 1000u,
                  YESNO(config.enabled), YESNO(config.recharge));
  }
  if (this->thermistor_ != NPM1300_UNSET) {
    ESP_LOGCONFIG(TAG, "  Thermistor B constant: %u", this->thermistor_beta_);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Battery Current", this->battery_current_sensor_);
  LOG_SENSOR("  ", "Battery Temperature", this->battery_temperature_sensor_);
  LOG_SENSOR("  ", "Die Temperature", this->die_temperature_sensor_);
  LOG_SENSOR("  ", "System Voltage", this->system_voltage_sensor_);
  LOG_SENSOR("  ", "VBUS Voltage", this->vbus_voltage_sensor_);
  LOG_SENSOR("  ", "VBUS Current Limit", this->vbus_current_limit_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "VBUS Present", this->vbus_present_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "VBUS Current Limit Active", this->vbus_current_limit_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Charging", this->charging_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Charge Complete", this->charge_complete_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Battery Detected", this->battery_detected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Charger Error", this->charger_error_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Charger Status", this->charger_status_text_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Charger", this->charger_switch_);
  LOG_SWITCH("  ", "BUCK1", this->buck1_switch_);
  LOG_SWITCH("  ", "BUCK2", this->buck2_switch_);
  LOG_SWITCH("  ", "LDO1", this->ldo1_switch_);
  LOG_SWITCH("  ", "LDO2", this->ldo2_switch_);
#endif
}

}  // namespace esphome::npm1300
