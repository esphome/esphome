#include "bq25820.h"
#ifdef BQ25820_BINARY_SENSOR_COUNT
#include "binary_sensor/bq25820_binary_sensor.h"
#endif
#ifdef BQ25820_TEXT_SENSOR_COUNT
#include "text_sensor/bq25820_text_sensor.h"
#endif

#include <cmath>
#include <iterator>

#include "esphome/core/log.h"

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
#ifdef BQ25820_BINARY_SENSOR_COUNT
  for (auto *sensor : this->binary_sensors_)
    LOG_BINARY_SENSOR("  ", "Binary Sensor", sensor);
#endif
#ifdef BQ25820_TEXT_SENSOR_COUNT
  for (auto *sensor : this->text_sensors_)
    LOG_TEXT_SENSOR("  ", "Text Sensor", sensor);
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
  BQ25820StatusRegisters status;
  if (!this->status_comm_result(this->read_bytes(REG_CHARGER_STATUS_1, status.data(), status.size())))
    return;

  // WD_STAT: the watchdog expired and reset the configuration
  if (!this->config_written_ || (this->watchdog_enabled_() && (status[0] & 0x08) != 0))
    this->config_written_ = this->write_config_registers_();

#ifdef BQ25820_BINARY_SENSOR_COUNT
  for (auto *sensor : this->binary_sensors_)
    sensor->on_status(status);
#endif
#ifdef BQ25820_TEXT_SENSOR_COUNT
  for (auto *sensor : this->text_sensors_)
    sensor->on_status(status);
#endif
  this->publish_adc_();
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
