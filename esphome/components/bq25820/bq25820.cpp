#include "bq25820.h"

#include <cmath>
#include <iterator>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::bq25820 {

ESPHOME_LOG_TAG(TAG, "bq25820");

static constexpr uint8_t REG_CHARGE_VOLTAGE_LIMIT = 0x00;
static constexpr uint8_t REG_CHARGE_CURRENT_LIMIT = 0x02;
static constexpr uint8_t REG_INPUT_CURRENT_LIMIT = 0x06;
static constexpr uint8_t REG_INPUT_VOLTAGE_LIMIT = 0x08;
static constexpr uint8_t REG_REVERSE_MODE_VOLTAGE_LIMIT = 0x0C;
static constexpr uint8_t REG_PRECHARGE_CURRENT_LIMIT = 0x10;
static constexpr uint8_t REG_TIMER_CONTROL = 0x15;
static constexpr uint8_t REG_CHARGER_CONTROL = 0x17;
static constexpr uint8_t REG_POWER_PATH_CONTROL = 0x19;
static constexpr uint8_t REG_MPPT_CONTROL = 0x1A;
static constexpr uint8_t REG_TS_THRESHOLD_CONTROL = 0x1B;
static constexpr uint8_t REG_TS_REGION_CONTROL = 0x1C;
static constexpr uint8_t REG_CHARGER_STATUS_1 = 0x21;
static constexpr uint8_t REG_ADC_CONTROL = 0x2B;
static constexpr uint8_t REG_IAC_ADC = 0x2D;
static constexpr uint8_t REG_PART_INFORMATION = 0x3D;

// PART_NUM 011b (BQ25820), DEV_REV 010b
static constexpr uint8_t PART_INFORMATION = 0x1A;

// Charger control (0x17)
static constexpr uint8_t WD_RST = 0x20;
// Power path control (0x19)
static constexpr uint8_t REG_RST = 0x80;
static constexpr uint8_t EN_BATFET_IDEAL_DIODE = 0x04;
// MPPT control (0x1A)
static constexpr uint8_t FORCE_SWEEP = 0x80;
static constexpr uint8_t EN_MPPT = 0x01;
// ADC control (0x2B): ADC_EN set, ADC_RATE cleared for continuous conversion
static constexpr uint8_t ADC_EN_RATE_MASK = 0xC0;
static constexpr uint8_t ADC_EN = 0x80;

// The watchdog is set to 40 s; feed it well before it expires
static constexpr uint32_t WATCHDOG_FEED_INTERVAL_MS = 15000;

static constexpr float VOLTAGE_ADC_STEP_V = 0.002f;
// A current limit step is 250 uV across the sense resistor: 25 IBAT or 62.5 IAC ADC steps
static constexpr float CHARGE_LIMIT_ADC_STEPS = 25.0f;
static constexpr float INPUT_LIMIT_ADC_STEPS = 62.5f;

// TS pin NTC: 10k to PGND with a 10k reference resistor, B = 3435
static constexpr float NTC_R0_OHM = 10000.0f;
static constexpr float NTC_REF_OHM = 10000.0f;
static constexpr float NTC_B = 3435.0f;

// CHARGE_STAT[2:0]
static constexpr uint8_t CHARGE_STAT_NOT_CHARGING = 0;
static constexpr uint8_t CHARGE_STAT_TRICKLE = 1;
static constexpr uint8_t CHARGE_STAT_PRECHARGE = 2;
static constexpr uint8_t CHARGE_STAT_RESERVED = 5;
static constexpr uint8_t CHARGE_STAT_DONE = 7;

#ifdef USE_TEXT_SENSOR
// Code 5 is reserved
PROGMEM_STRING_TABLE(ChargeStatusStrings, "Not Charging", "Trickle", "Precharge", "Fast Charge", "Taper", "Unknown",
                     "Top-Off", "Done");
// Codes 5-7 are not defined
PROGMEM_STRING_TABLE(TempStatusStrings, "Normal", "Warm", "Cool", "Cold", "Hot", "Unknown");
#endif

void BQ25820Component::setup() {
  if (!this->check_chip_id(this, REG_PART_INFORMATION, PART_INFORMATION))
    return;

  // Start from the reset values; the bit clears itself
  if (!this->update_register(REG_POWER_PATH_CONTROL, REG_RST, REG_RST)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // A failed write is retried from update()
  this->config_written_ = this->write_config_registers_();

  // Runs one panel sweep at startup
  if (this->force_mppt_sweep_)
    this->update_register(REG_MPPT_CONTROL, FORCE_SWEEP, FORCE_SWEEP);

  if (this->watchdog_enabled_()) {
    this->set_interval(WATCHDOG_FEED_INTERVAL_MS,
                       [this]() { this->write_byte(REG_CHARGER_CONTROL, this->charger_control_ | WD_RST); });
  }

  this->update();
}

void BQ25820Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BQ25820:\n"
                "  Watchdog: %s\n"
                "  MPPT: %s",
                YESNO(this->watchdog_enabled_()), YESNO(this->mppt_control_ & EN_MPPT));
  if (this->charge_current_code_ != 0) {
    ESP_LOGCONFIG(TAG, "  Charge current: %.2f A",
                  this->charge_current_code_ * CHARGE_LIMIT_ADC_STEPS * this->ibat_scale_);
  }
  if (this->precharge_current_code_ != 0) {
    ESP_LOGCONFIG(TAG, "  Precharge current: %.2f A",
                  this->precharge_current_code_ * CHARGE_LIMIT_ADC_STEPS * this->ibat_scale_);
  }
  if (this->input_current_limit_code_ != 0) {
    ESP_LOGCONFIG(TAG, "  Input current limit: %.2f A",
                  this->input_current_limit_code_ * INPUT_LIMIT_ADC_STEPS * this->iac_scale_);
  }
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Battery Voltage", this->vbat_sensor_);
  LOG_SENSOR("  ", "System Voltage", this->vsys_sensor_);
  LOG_SENSOR("  ", "Input Voltage", this->vac_sensor_);
  LOG_SENSOR("  ", "Battery Current", this->ibat_sensor_);
  LOG_SENSOR("  ", "Input Current", this->iac_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temp_in_c_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Charging", this->charging_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Charged", this->charged_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Low Battery", this->low_battery_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Power Good", this->pg_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Reverse", this->reverse_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Input Undervoltage", this->vac_undervoltage_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Input Overvoltage", this->vac_overvoltage_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Battery Overcurrent", this->ibat_overcurrent_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Battery Overvoltage", this->vbat_overvoltage_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Thermal Shutdown", this->thermal_shutdown_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Charge Safety Timer", this->charge_safety_timer_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Driver Supply Invalid", this->drv_supply_invalid_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Charge Status", this->charge_status_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Temperature Status", this->temp_status_text_sensor_);
#endif
}

// Called from setup() after the register reset, and again after a failed write or a watchdog expiry.
bool BQ25820Component::write_config_registers_() {
  bool ok = true;
  // 16-bit limits hold their code from bit 2 up
  if (this->charge_current_code_ != 0)
    ok &= this->write_byte_16_le(REG_CHARGE_CURRENT_LIMIT, this->charge_current_code_ << 2);
  if (this->precharge_current_code_ != 0)
    ok &= this->write_byte_16_le(REG_PRECHARGE_CURRENT_LIMIT, this->precharge_current_code_ << 2);
  if (this->input_current_limit_code_ != 0)
    ok &= this->write_byte_16_le(REG_INPUT_CURRENT_LIMIT, this->input_current_limit_code_ << 2);
  ok &= this->write_byte_16_le(REG_INPUT_VOLTAGE_LIMIT, this->input_voltage_limit_code_ << 2);
  ok &= this->write_byte_16_le(REG_REVERSE_MODE_VOLTAGE_LIMIT, this->reverse_system_voltage_code_ << 2);
  ok &= this->update_register(REG_CHARGE_VOLTAGE_LIMIT, 0x1F, this->charge_voltage_code_);
  // Everything but EN_TMR2X
  ok &= this->update_register(REG_TIMER_CONTROL, 0xFE, this->timer_control_);
  ok &= this->write_byte(REG_CHARGER_CONTROL, this->charger_control_);
  ok &= this->update_register(REG_POWER_PATH_CONTROL, EN_BATFET_IDEAL_DIODE,
                              this->ideal_diode_enabled_ ? EN_BATFET_IDEAL_DIODE : 0);
  // FULL_SWEEP_TMR[2:1], EN_MPPT
  ok &= this->update_register(REG_MPPT_CONTROL, 0x07, this->mppt_control_);
  ok &= this->write_byte(REG_TS_THRESHOLD_CONTROL, this->ts_thresholds_);
  // EN_TS
  ok &= this->update_register(REG_TS_REGION_CONTROL, 0x01, this->temp_enabled_ ? 1 : 0);
  ok &= this->update_register(REG_ADC_CONTROL, ADC_EN_RATE_MASK, ADC_EN);
  if (!ok) {
    ESP_LOGW(TAG, "Configuration write failed");
  }
  return ok;
}

void BQ25820Component::update() {
  // CHARGER_STATUS_1-3 and FAULT_STATUS (0x21-0x24)
  uint8_t status[4];
  if (!this->status_comm_result(this->read_bytes(REG_CHARGER_STATUS_1, status, sizeof(status))))
    return;

  // WD_STAT: the watchdog expired and reset the configuration
  if (!this->config_written_ || (this->watchdog_enabled_() && (status[0] & 0x08) != 0))
    this->config_written_ = this->write_config_registers_();

  this->publish_status_(status);
  this->publish_adc_();
}

void BQ25820Component::publish_status_(const uint8_t *status) {
  [[maybe_unused]] const uint8_t charge_stat = status[0] & 0x07;
  [[maybe_unused]] const uint8_t ts_stat = (status[1] >> 4) & 0x07;
#ifdef USE_BINARY_SENSOR
  const uint8_t fault = status[3];
  auto publish = [](binary_sensor::BinarySensor *sensor, bool state) {
    if (sensor != nullptr)
      sensor->publish_state(state);
  };
  publish(this->charging_binary_sensor_, charge_stat != CHARGE_STAT_NOT_CHARGING && charge_stat != CHARGE_STAT_DONE);
  publish(this->charged_binary_sensor_, charge_stat == CHARGE_STAT_DONE);
  // Trickle and precharge both mean the battery is below VBAT_LOWV
  publish(this->low_battery_binary_sensor_, charge_stat == CHARGE_STAT_TRICKLE || charge_stat == CHARGE_STAT_PRECHARGE);
  publish(this->pg_binary_sensor_, status[1] & 0x80);
  publish(this->reverse_binary_sensor_, status[2] & 0x04);
  publish(this->vac_undervoltage_binary_sensor_, fault & 0x80);
  publish(this->vac_overvoltage_binary_sensor_, fault & 0x40);
  publish(this->ibat_overcurrent_binary_sensor_, fault & 0x20);
  publish(this->vbat_overvoltage_binary_sensor_, fault & 0x10);
  publish(this->thermal_shutdown_binary_sensor_, fault & 0x08);
  publish(this->charge_safety_timer_binary_sensor_, fault & 0x04);
  publish(this->drv_supply_invalid_binary_sensor_, fault & 0x02);
#endif
#ifdef USE_TEXT_SENSOR
  if (this->charge_status_text_sensor_ != nullptr && charge_stat != this->last_charge_stat_) {
    this->last_charge_stat_ = charge_stat;
    this->charge_status_text_sensor_->publish_state(
        ChargeStatusStrings::get_progmem_str(charge_stat, CHARGE_STAT_RESERVED));
  }
  if (this->temp_status_text_sensor_ != nullptr && ts_stat != this->last_ts_stat_) {
    this->last_ts_stat_ = ts_stat;
    this->temp_status_text_sensor_->publish_state(
        TempStatusStrings::get_progmem_str(ts_stat, TempStatusStrings::LAST_INDEX));
  }
#endif
}

void BQ25820Component::publish_adc_() {
#ifdef USE_SENSOR
  // IAC, IBAT, VAC, VBAT, VSYS and TS ADCs (0x2D-0x38)
  int16_t adc[6];
  if (!this->status_comm_result(this->read_bytes_16_le(REG_IAC_ADC, adc, std::size(adc))))
    return;
  if (this->iac_sensor_ != nullptr)
    this->iac_sensor_->publish_state(adc[0] * this->iac_scale_);
  // Positive while charging
  if (this->ibat_sensor_ != nullptr)
    this->ibat_sensor_->publish_state(adc[1] * this->ibat_scale_);
  if (this->vac_sensor_ != nullptr)
    this->vac_sensor_->publish_state(static_cast<uint16_t>(adc[2]) * VOLTAGE_ADC_STEP_V);
  if (this->vbat_sensor_ != nullptr)
    this->vbat_sensor_->publish_state(static_cast<uint16_t>(adc[3]) * VOLTAGE_ADC_STEP_V);
  if (this->vsys_sensor_ != nullptr)
    this->vsys_sensor_->publish_state(static_cast<uint16_t>(adc[4]) * VOLTAGE_ADC_STEP_V);
  if (this->temp_in_c_sensor_ != nullptr) {
    // TS_ADC is the TS pin voltage as a 10-bit share of REGN
    const float ratio = (adc[5] & 0x3FF) / 1024.0f;
    float temperature = NAN;
    if (ratio > 0.0f) {
      const float r_ntc = NTC_REF_OHM * ratio / (1.0f - ratio);
      temperature = 1.0f / (1.0f / 298.15f + logf(r_ntc / NTC_R0_OHM) / NTC_B) - 273.15f;
    }
    this->temp_in_c_sensor_->publish_state(temperature);
  }
#endif
}

}  // namespace esphome::bq25820
