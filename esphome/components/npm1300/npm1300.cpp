#include "npm1300.h"
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include "esphome/core/hal.h"
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
static constexpr uint16_t REG_BCHG_TASK_CLEAR_CHG_ERR = 0x0301;
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
// BCHGCHARGESTATUS; bit 0 (battery detected) is reserved since C00 silicon
static constexpr uint8_t CHARGE_STATUS_COMPLETED = 0x02;
static constexpr uint8_t CHARGE_STATUS_TRICKLE = 0x04;
static constexpr uint8_t CHARGE_STATUS_CONSTANT_CURRENT = 0x08;
static constexpr uint8_t CHARGE_STATUS_CONSTANT_VOLTAGE = 0x10;
static constexpr uint8_t CHARGE_STATUS_DIE_TEMP_HIGH = 0x40;
// BCHGDISABLESET and BCHGDISABLECLR
static constexpr uint8_t CHARGER_DISABLE_RECHARGE = 0x01;
static constexpr uint8_t CHARGER_DISABLE_NTC = 0x02;
// BUCKSTATUS power good, LDSWSTATUS powered up as load switch or as LDO
static constexpr uint8_t BUCK_POWER_GOOD[2] = {0x04, 0x40};
static constexpr uint8_t LDSW_POWERED[2] = {0x03, 0x0C};
// TIMERCONFIG: wake-up timer with the 16 ms prescaler
static constexpr uint8_t TIMER_CONFIG_WAKEUP = 0x04;
static constexpr uint32_t TIMER_TICK_MS = 16;
static constexpr uint32_t TIMER_MAX_TICKS = 0xFFFFFF;

// BCHGISETDISCHARGE codes up to this one select the 200 mA discharge limit, the others 1000 mA
static constexpr uint16_t DISCHARGE_LIMIT_200MA_CODE = 84;

static uint16_t discharge_limit_ma(uint16_t code) { return code <= DISCHARGE_LIMIT_200MA_CODE ? 200 : 1000; }

// BCHGISET and BCHGISETDISCHARGE hold a 9-bit code: the high 8 bits, then bit 0 in the next register
static void split_9bit(uint16_t code, uint8_t *out) {
  out[0] = code >> 1;
  out[1] = code & 1;
}

#ifdef USE_NPM1300_DATA_PUBLISHER
static uint16_t join_9bit(const uint8_t *in) { return (in[0] << 1) | (in[1] & 1); }

// ADCIBATMEASSTATUS
static constexpr uint8_t IBAT_MEAS_INVALID = 0x10;
static constexpr uint8_t IBAT_MODE_SHIFT = 2;
static constexpr uint8_t IBAT_MODE_DISCHARGE = 0x01;
static constexpr uint8_t IBAT_MODE_CHARGE = 0x03;
// Full scale of the battery current measurement per milliampere of the charge and discharge limits
static constexpr float CHARGE_FULL_SCALE_FACTOR = 1.25f;
static constexpr float DISCHARGE_FULL_SCALE_FACTOR = 1.12f;
static constexpr float ADC_MAX_CODE = 1023.0f;
// Five conversions of 250 us each; the loop runs the timeout later than this anyway
static constexpr uint32_t CONVERSION_TIME_MS = 2;

static constexpr uint32_t READ_TIMEOUT_ID = 0;

float ntc_temperature(uint16_t code, float inv_beta) {
  if (code == 0)
    return NAN;
  // The NTC divider gives code = 1024 * R / (R + R_nominal)
  return 1.0f / (1.0f / 298.15f - logf(1024.0f / code - 1.0f) * inv_beta) - 273.15f;
}
#endif

#ifdef USE_TEXT_SENSOR
PROGMEM_STRING_TABLE(ChargerStatusStrings, "Error", "Paused, Die Too Hot", "Charge Complete", "Trickle Charge",
                     "Constant Current", "Constant Voltage", "Discharging", "Not Charging");
// BCHGCHARGESTATUS bits in order of priority; bit i is reported as string i + 1
static const uint8_t CHARGER_STATUS_BITS[] PROGMEM = {CHARGE_STATUS_DIE_TEMP_HIGH, CHARGE_STATUS_COMPLETED,
                                                      CHARGE_STATUS_TRICKLE, CHARGE_STATUS_CONSTANT_CURRENT,
                                                      CHARGE_STATUS_CONSTANT_VOLTAGE};

static uint8_t charger_status_index(uint8_t vbus_status, uint8_t charge_status, uint8_t error_reason) {
  if (error_reason != 0)
    return 0;
  for (size_t i = 0; i < sizeof(CHARGER_STATUS_BITS); i++) {
    if ((charge_status & progmem_read_byte(&CHARGER_STATUS_BITS[i])) != 0)
      return i + 1;
  }
  return (vbus_status & VBUS_PRESENT) == 0 ? 6 : 7;
}

void publish_charger_status(text_sensor::TextSensor *sensor, uint8_t &last, uint8_t vbus_status, uint8_t charge_status,
                            uint8_t error_reason) {
  const uint8_t index = charger_status_index(vbus_status, charge_status, error_reason);
  if (index == last)
    return;
  last = index;
  sensor->publish_state(ChargerStatusStrings::get_progmem_str(index, ChargerStatusStrings::LAST_INDEX));
}
#endif

NPM1300Config NPM1300Component::load_config_() const {
  NPM1300Config config;
  progmem_memcpy(&config, this->config_, sizeof(config));
  return config;
}

void NPM1300Component::setup() {
  uint8_t status;
  if (!this->setup_check(this->read_byte_reg16(REG_BCHG_CHARGE_STATUS, &status)))
    return;

  // Raise the VBUS current limit before anything turns on a regulator: at the power-on limit of 100 mA the
  // inrush current of a regulator can trip the over-current protection and brown out the board
  if (this->vbus_current_limit_ != NPM1300_UNSET &&
      !this->setup_check(this->write_byte_reg16(REG_VBUSIN_ILIM_STARTUP, this->vbus_current_limit_) &&
                         this->apply_vbus_current_limit_()))
    return;

  const NPM1300Config config = this->load_config_();
  if (config.thermistor != NPM1300_UNSET &&
      !this->setup_check(this->write_byte_reg16(REG_ADC_NTC_R_SEL, config.thermistor)))
    return;

  if (config.charger_configured && !this->setup_check(this->configure_charger_(config)))
    return;

  for (uint8_t i = 0; i < NPM1300_OUTPUT_COUNT; i++) {
    if (!this->setup_check(this->configure_output_(i, config.outputs[i])))
      return;
  }

#ifdef USE_NPM1300_DATA_PUBLISHER
  if (this->data_publisher_ != nullptr) {
    // The battery current is measured after every battery voltage measurement
    if (!this->setup_check(this->write_byte_reg16(REG_ADC_IBAT_MEAS_EN, 1)))
      return;
    if (config.charger_configured) {
      this->charge_code_ = config.charger.current;
      this->discharge_limit_high_ = discharge_limit_ma(config.charger.discharge_limit) == 1000;
    } else if (!this->setup_check(this->read_charge_currents_())) {
      return;
    }
  }
#endif

  this->update();
}

bool NPM1300Component::apply_vbus_current_limit_() {
  return this->write_byte_reg16(REG_VBUSIN_ILIM0, this->vbus_current_limit_) &&
         this->write_byte_reg16(REG_VBUSIN_TASK_UPDATE_ILIM, 1);
}

bool NPM1300Component::configure_charger_(const NPM1300Config &config) {
  const NPM1300ChargerConfig &charger = config.charger;
  // The charge settings are only taken while the charger is off
  if (!this->write_byte_reg16(REG_BCHG_ENABLE_CLR, 1))
    return false;
  uint8_t current[2];
  uint8_t discharge[2];
  split_9bit(charger.current, current);
  split_9bit(charger.discharge_limit, discharge);
  uint8_t disable = charger.recharge ? 0 : CHARGER_DISABLE_RECHARGE;
  // Without a thermistor the charger reports an NTC error and does not charge
  if (config.thermistor == NPM1300_UNSET)
    disable |= CHARGER_DISABLE_NTC;
  const uint8_t enable = (CHARGER_DISABLE_RECHARGE | CHARGER_DISABLE_NTC) & ~disable;
  if (this->write_register16(REG_BCHG_ISET_MSB, current, sizeof(current)) != i2c::ERROR_OK ||
      this->write_register16(REG_BCHG_ISET_DISCHARGE_MSB, discharge, sizeof(discharge)) != i2c::ERROR_OK ||
      !this->write_byte_reg16(REG_BCHG_VTERM, charger.termination_voltage) ||
      !this->write_byte_reg16(REG_BCHG_VTERM_R, charger.warm_termination_voltage) ||
      !this->write_byte_reg16(REG_BCHG_VTRICKLE_SEL, charger.trickle_voltage) ||
      !this->write_byte_reg16(REG_BCHG_ITERM_SEL, charger.termination_current) ||
      !this->write_byte_reg16(REG_BCHG_DISABLE_SET, disable) || !this->write_byte_reg16(REG_BCHG_DISABLE_CLR, enable) ||
      // Measure the battery temperature automatically while charging
      !this->write_byte_reg16(REG_ADC_TASK_AUTO_TIM_UPDATE, 1))
    return false;
  return !charger.enabled || this->set_charger_enabled(true);
}

bool NPM1300Component::configure_output_(uint8_t index, const NPM1300OutputConfig &config) {
  const uint8_t channel = index & 1;
  if (index < 2) {
    // The software voltage replaces the one set by the VSET pin only once its SWCTRLSEL bit is set
    if (config.voltage != NPM1300_UNSET &&
        (!this->write_byte_reg16(REG_BUCK1_NORM_VOUT + channel * 2, config.voltage) ||
         !this->update_register16(REG_BUCK_SW_CTRL_SEL, 1 << channel, 1 << channel)))
      return false;
  } else if ((config.voltage != NPM1300_UNSET &&
              !this->write_byte_reg16(REG_LDSW1_VOUT_SEL + channel, config.voltage)) ||
             (config.ldo_mode != NPM1300_UNSET &&
              !this->write_byte_reg16(REG_LDSW1_LDO_SEL + channel, config.ldo_mode))) {
    return false;
  }
  return config.enabled == NPM1300_UNSET ||
         this->set_output_enabled(static_cast<NPM1300Output>(index), config.enabled != 0);
}

#ifdef USE_NPM1300_DATA_PUBLISHER
bool NPM1300Component::read_charge_currents_() {
  uint8_t charge[2];
  uint8_t discharge[2];
  if (this->read_register16(REG_BCHG_ISET_MSB, charge, sizeof(charge)) != i2c::ERROR_OK ||
      this->read_register16(REG_BCHG_ISET_DISCHARGE_MSB, discharge, sizeof(discharge)) != i2c::ERROR_OK)
    return false;
  this->charge_code_ = join_9bit(charge);
  this->discharge_limit_high_ = discharge_limit_ma(join_9bit(discharge)) == 1000;
  return true;
}
#endif

bool NPM1300Component::set_charger_enabled(bool enabled) {
  if (this->is_failed())
    return false;
  if (!enabled)
    return this->write_byte_reg16(REG_BCHG_ENABLE_CLR, 1);
  // A charger error stops the charger until the error is cleared and the charger released
  return this->write_byte_reg16(REG_BCHG_TASK_CLEAR_CHG_ERR, 1) &&
         this->write_byte_reg16(REG_BCHG_TASK_RELEASE_ERR, 1) && this->write_byte_reg16(REG_BCHG_ENABLE_SET, 1);
}

bool NPM1300Component::set_output_enabled(NPM1300Output output, bool enabled) {
  if (this->is_failed())
    return false;
  const uint8_t index = static_cast<uint8_t>(output);
  // A pair of SET and CLR task registers per output
  const uint16_t base = index < 2 ? REG_BUCK1_ENA_SET : REG_LDSW1_TASK_SET;
  const uint16_t reg = base + (index & 1) * 2 + (enabled ? 0 : 1);
  if (!this->write_byte_reg16(reg, 1)) {
    ESP_LOGW(TAG, "Turning output %u %s failed", index, enabled ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"));
    return false;
  }
  return true;
}

void NPM1300Component::enter_ship_mode() {
  ESP_LOGI(TAG, "Entering ship mode");
  this->write_byte_reg16(REG_SHIP_TASK_SHIP_MODE, 1);
}

void NPM1300Component::hibernate(uint32_t duration_ms) {
  // A zero period would never wake the chip
  const uint32_t ticks = std::clamp<uint32_t>(duration_ms / TIMER_TICK_MS, 1, TIMER_MAX_TICKS);
  uint8_t timer[3];
  put_be24(timer, ticks);
  ESP_LOGI(TAG, "Hibernating for %" PRIu32 " ms", ticks * TIMER_TICK_MS);
  if (!this->write_byte_reg16(REG_TIMER_CONFIG, TIMER_CONFIG_WAKEUP) ||
      this->write_register16(REG_TIMER_HI_BYTE, timer, sizeof(timer)) != i2c::ERROR_OK ||
      !this->write_byte_reg16(REG_TIMER_TARGET_STROBE, 1)) {
    ESP_LOGW(TAG, "Setting the wake-up timer failed");
    return;
  }
  // The timer needs a moment to load the new period
  delay(1);
  this->write_byte_reg16(REG_SHIP_TASK_HIBERNATE, 1);
}

void NPM1300Component::update() {
  uint8_t vbus_status;
  uint8_t charge_status;
  uint8_t error_reason;
  if (!this->status_comm_check(this->read_byte_reg16(REG_VBUSIN_STATUS, &vbus_status) &&
                               this->read_byte_reg16(REG_BCHG_CHARGE_STATUS, &charge_status) &&
                               this->read_byte_reg16(REG_BCHG_ERR_REASON, &error_reason)))
    return;

  // The chip goes back to its default VBUS current limit whenever VBUS is removed
  const bool vbus_present = (vbus_status & VBUS_PRESENT) != 0;
  if (vbus_present && !this->vbus_present_ && this->vbus_current_limit_ != NPM1300_UNSET &&
      !this->status_comm_check(this->apply_vbus_current_limit_()))
    return;
  this->vbus_present_ = vbus_present;

  if (error_reason != this->last_error_reason_) {
    this->last_error_reason_ = error_reason;
    if (error_reason != 0) {
      ESP_LOGW(TAG, "Charger error, BCHGERRREASON 0x%02X", error_reason);
    }
  }
#ifdef USE_NPM1300_STATUS_PUBLISHER
  if (this->status_publisher_ != nullptr)
    this->status_publisher_(vbus_status, charge_status, error_reason);
#endif
#ifdef USE_SWITCH
  if (!this->status_comm_check(this->publish_switches_()))
    return;
#endif

#ifdef USE_SENSOR
  if (this->vbus_current_limit_sensor_ != nullptr) {
    uint8_t code;
    if (!this->status_comm_check(this->read_byte_reg16(REG_VBUSIN_ILIM0, &code)))
      return;
    // 100 mA per step of the code, but code 0 is 500 mA
    const uint16_t milliamps = code == 0 ? 500 : code * 100;
    this->vbus_current_limit_sensor_->publish_state(milliamps / 1000.0f);
  }
#endif
#ifdef USE_NPM1300_DATA_PUBLISHER
  if (this->data_publisher_ != nullptr) {
    // Start the battery voltage (followed by the battery current), NTC, die temperature and system voltage
    // conversions, then the VBUS one
    static constexpr uint8_t START[4] = {1, 1, 1, 1};
    if (!this->status_comm_check(this->write_register16(REG_ADC_TASK_VBAT, START, sizeof(START)) == i2c::ERROR_OK &&
                                 this->write_byte_reg16(REG_ADC_TASK_VBUS, 1)))
      return;
    this->set_timeout(READ_TIMEOUT_ID, CONVERSION_TIME_MS, [this]() { this->read_measurements_(); });
    return;
  }
#endif
  this->status_comm_result(true);
}

#ifdef USE_SWITCH
bool NPM1300Component::publish_switches_() {
  uint8_t value;
  bool ok = true;
  if (this->charger_switch_ != nullptr) {
    if (this->read_byte_reg16(REG_BCHG_ENABLE_SET, &value)) {
      this->charger_switch_->publish_state((value & 1) != 0);
    } else {
      ok = false;
    }
  }
  switch_::Switch *const bucks[2] = {this->buck1_switch_, this->buck2_switch_};
  if (bucks[0] != nullptr || bucks[1] != nullptr) {
    if (this->read_byte_reg16(REG_BUCK_STATUS, &value)) {
      for (uint8_t i = 0; i < 2; i++)
        PUBLISH_STATE(bucks[i], (value & BUCK_POWER_GOOD[i]) != 0);
    } else {
      ok = false;
    }
  }
  switch_::Switch *const ldos[2] = {this->ldo1_switch_, this->ldo2_switch_};
  if (ldos[0] != nullptr || ldos[1] != nullptr) {
    if (this->read_byte_reg16(REG_LDSW_STATUS, &value)) {
      for (uint8_t i = 0; i < 2; i++)
        PUBLISH_STATE(ldos[i], (value & LDSW_POWERED[i]) != 0);
    } else {
      ok = false;
    }
  }
  return ok;
}
#endif

#ifdef USE_NPM1300_DATA_PUBLISHER
void NPM1300Component::read_measurements_() {
  // ADCIBATMEASSTATUS, then the VBAT, NTC, TEMP and VSYS results, their low bits, VBAT0 to VBAT3 (VBAT2 holds the
  // battery current and VBAT3 the VBUS voltage) and their low bits
  uint8_t adc[11];
  if (!this->status_comm_result(this->read_register16(REG_ADC_RESULTS, adc, sizeof(adc)) == i2c::ERROR_OK))
    return;
  // Amperes per code of the battery current, positive while charging and negative while discharging
  float ibat_scale = NAN;
  // Any other mode means the system runs from VBUS and the battery current is not measured
  const uint8_t mode = (adc[0] >> IBAT_MODE_SHIFT) & 0x03;
  if ((adc[0] & IBAT_MEAS_INVALID) == 0) {
    if (mode == IBAT_MODE_CHARGE) {
      ibat_scale = this->charge_code_ * (2 * CHARGE_FULL_SCALE_FACTOR) / (ADC_MAX_CODE * 1000.0f);
    } else if (mode == IBAT_MODE_DISCHARGE) {
      ibat_scale = -(this->discharge_limit_high_ ? 1000 : 200) * DISCHARGE_FULL_SCALE_FACTOR / (ADC_MAX_CODE * 1000.0f);
    }
  }
  if (this->data_publisher_ != nullptr)
    this->data_publisher_(adc, ibat_scale);
}
#endif

// BCHGVTERM: 3.50 V to 3.65 V, then 4.00 V to 4.45 V, both in 50 mV steps
static uint16_t termination_voltage_mv(uint8_t code) { return code < 4 ? 3500 + 50 * code : 4000 + 50 * (code - 4); }

static const LogString *output_name(uint8_t index) {
  switch (static_cast<NPM1300Output>(index)) {
    case NPM1300Output::NPM1300_OUTPUT_BUCK1:
      return LOG_STR("BUCK1");
    case NPM1300Output::NPM1300_OUTPUT_BUCK2:
      return LOG_STR("BUCK2");
    case NPM1300Output::NPM1300_OUTPUT_LDO1:
      return LOG_STR("LDO1");
    default:
      return LOG_STR("LDO2");
  }
}

void NPM1300Component::dump_config() {
  ESP_LOGCONFIG(TAG, "nPM1300:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->vbus_current_limit_ != NPM1300_UNSET) {
    // 100 mA per step of the code
    ESP_LOGCONFIG(TAG, "  VBUS current limit: %u mA", this->vbus_current_limit_ * 100u);
  }
  const NPM1300Config config = this->load_config_();
  if (config.charger_configured) {
    const NPM1300ChargerConfig &charger = config.charger;
    ESP_LOGCONFIG(TAG,
                  "  Charger:\n"
                  "    Current: %u mA\n"
                  "    Discharge limit: %u mA\n"
                  "    Termination voltage: %u mV\n"
                  "    Warm termination voltage: %u mV\n"
                  "    Termination current: %u%%\n"
                  "    Trickle voltage: %u mV\n"
                  "    Enabled: %s\n"
                  "    Recharge: %s",
                  charger.current * 2u, discharge_limit_ma(charger.discharge_limit),
                  termination_voltage_mv(charger.termination_voltage),
                  termination_voltage_mv(charger.warm_termination_voltage), charger.termination_current ? 20u : 10u,
                  charger.trickle_voltage ? 2500u : 2900u, YESNO(charger.enabled), YESNO(charger.recharge));
  }
  for (uint8_t i = 0; i < NPM1300_OUTPUT_COUNT; i++) {
    const NPM1300OutputConfig &output = config.outputs[i];
    if (output.voltage == NPM1300_UNSET && output.ldo_mode == NPM1300_UNSET && output.enabled == NPM1300_UNSET)
      continue;
    ESP_LOGCONFIG(TAG, "  %s:", LOG_STR_ARG(output_name(i)));
    if (output.voltage != NPM1300_UNSET) {
      ESP_LOGCONFIG(TAG, "    Voltage: %u mV", 1000u + output.voltage * 100u);
    }
    if (output.ldo_mode != NPM1300_UNSET) {
      ESP_LOGCONFIG(TAG, "    Mode: %s",
                    output.ldo_mode != 0 ? LOG_STR_LITERAL("LDO") : LOG_STR_LITERAL("load switch"));
    }
    if (output.enabled != NPM1300_UNSET) {
      ESP_LOGCONFIG(TAG, "    Enabled at boot: %s", YESNO(output.enabled != 0));
    }
  }
  if (config.thermistor != NPM1300_UNSET) {
    ESP_LOGCONFIG(TAG, "  Thermistor B constant: %u", config.thermistor_beta);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "VBUS Current Limit", this->vbus_current_limit_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Charger", this->charger_switch_);
  LOG_SWITCH("  ", "BUCK1", this->buck1_switch_);
  LOG_SWITCH("  ", "BUCK2", this->buck2_switch_);
  LOG_SWITCH("  ", "LDO1", this->ldo1_switch_);
  LOG_SWITCH("  ", "LDO2", this->ldo2_switch_);
#endif
#if defined(USE_NPM1300_STATUS_PUBLISHER) || defined(USE_NPM1300_DATA_PUBLISHER)
  if (this->dump_publisher_ != nullptr)
    this->dump_publisher_(TAG);
#endif
}

}  // namespace esphome::npm1300
