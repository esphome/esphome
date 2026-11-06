#include "bq25820.h"

#include "esphome/core/hal.h"
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
static constexpr uint8_t REG_PART_INFORMATION = 0x3D;

// PART_NUM[5:3] 011b (BQ25820); DEV_REV[2:0] is not checked, so a new silicon revision still passes
static constexpr uint8_t PART_NUM_BQ25820 = 0x18;
static constexpr uint8_t PART_NUM_MASK = 0x38;

// The 16-bit limits, written LSB first, in BQ25820Config::limits order
static constexpr uint8_t LIMIT_REGISTERS[] PROGMEM = {
    REG_CHARGE_CURRENT_LIMIT, REG_PRECHARGE_CURRENT_LIMIT,    REG_INPUT_CURRENT_LIMIT,
    REG_INPUT_VOLTAGE_LIMIT,  REG_REVERSE_MODE_VOLTAGE_LIMIT,
};
// The 8-bit registers in BQ25820Config::controls order, and the bits the configuration owns in each
struct ControlRegister {
  uint8_t reg;
  uint8_t mask;
};
static constexpr ControlRegister CONTROL_REGISTERS[] PROGMEM = {
    {REG_CHARGE_VOLTAGE_LIMIT, 0x1F},  // VFB_REG
    {REG_TIMER_CONTROL, 0xFE},         // everything but EN_TMR2X
    {REG_POWER_PATH_CONTROL, 0x04},    // EN_BATFET_IDEAL_DIODE
    {REG_MPPT_CONTROL, 0x07},          // FULL_SWEEP_TMR[2:1], EN_MPPT
    {REG_TS_THRESHOLD_CONTROL, 0xFF},  // TS_T1, TS_T2, TS_T3, TS_T5
    {REG_TS_REGION_CONTROL, 0x01},     // EN_TS
    {REG_CHARGER_CONTROL, 0xFF},       // VRECHG, EN_HIZ, EN_CHG; after every limit, so charging starts with them
};
static_assert(std::size(LIMIT_REGISTERS) == BQ25820_LIMIT_COUNT);
static_assert(std::size(CONTROL_REGISTERS) == BQ25820_CONTROL_COUNT);
static constexpr size_t CONTROL_TIMER = 1;
static constexpr size_t CONTROL_MPPT = 3;
static constexpr size_t CONTROL_CHARGER = 6;
static constexpr size_t LIMIT_CHARGE_CURRENT = 0;
static constexpr size_t LIMIT_PRECHARGE_CURRENT = 1;
static constexpr size_t LIMIT_INPUT_CURRENT = 2;

// Timer control (0x15)
static constexpr uint8_t TIMER_CONTROL_WATCHDOG = 0x30;
// Charger control (0x17)
static constexpr uint8_t WD_RST = 0x20;
static constexpr uint8_t EN_CHG = 0x01;
// Charger status 1 (0x21)
static constexpr uint8_t WD_STAT = 0x08;
// Power path control (0x19)
static constexpr uint8_t REG_RST = 0x80;
// MPPT control (0x1A)
static constexpr uint8_t FORCE_SWEEP = 0x80;
static constexpr uint8_t EN_MPPT = 0x01;
// ADC control (0x2B); the configuration sets it last and a charger reset clears it
static constexpr uint8_t ADC_EN = 0x80;
static constexpr uint8_t ADC_EN_RATE_MASK = 0xC0;

static constexpr uint32_t PUBLISH_TIMEOUT_ID = 0;
// After a configuration write, long enough for the ADC to finish its first round of conversions
static constexpr uint32_t FIRST_PUBLISH_DELAY_MS = 1000;

// A current limit step is 250 uV across the sense resistor: 25 IBAT or 62.5 IAC ADC steps
static constexpr float CHARGE_LIMIT_ADC_STEPS = 25.0f;
static constexpr float INPUT_LIMIT_ADC_STEPS = 62.5f;

static void log_current_limit(const LogString *name, uint16_t limit, float amps_per_code) {
  if (limit != 0) {
    ESP_LOGCONFIG(TAG, "  %s: %.2f A", LOG_STR_ARG(name), (limit >> 2) * amps_per_code);
  }
}

void BQ25820Component::setup() {
  if (!this->check_chip_id(this, REG_PART_INFORMATION, PART_NUM_BQ25820, PART_NUM_MASK))
    return;

  // Start from the reset values; the bit clears itself
  if (!this->setup_check(this->update_register(REG_POWER_PATH_CONTROL, REG_RST, REG_RST)))
    return;

  // Written now, whatever the reads do, so charging never runs at the reset limits for an interval
  this->configure_();
}

void BQ25820Component::dump_config() {
  const BQ25820Config config = this->load_config_();
  ESP_LOGCONFIG(TAG,
                "BQ25820:\n"
                "  Watchdog: %s\n"
                "  MPPT: %s",
                YESNO(this->watchdog_enabled_()), YESNO(config.controls[CONTROL_MPPT] & EN_MPPT));
  const float charge_step = CHARGE_LIMIT_ADC_STEPS * config.ibat_scale;
  log_current_limit(LOG_STR("Charge current"), config.limits[LIMIT_CHARGE_CURRENT], charge_step);
  log_current_limit(LOG_STR("Precharge current"), config.limits[LIMIT_PRECHARGE_CURRENT], charge_step);
  log_current_limit(LOG_STR("Input current limit"), config.limits[LIMIT_INPUT_CURRENT],
                    INPUT_LIMIT_ADC_STEPS * config.iac_scale);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_BQ25820_SENSOR
  LOG_SENSOR("  ", "Battery Voltage", this->vbat_sensor_);
  LOG_SENSOR("  ", "System Voltage", this->vsys_sensor_);
  LOG_SENSOR("  ", "Input Voltage", this->vac_sensor_);
  LOG_SENSOR("  ", "Battery Current", this->ibat_sensor_);
  LOG_SENSOR("  ", "Input Current", this->iac_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temp_in_c_sensor_);
#endif
#ifdef USE_BQ25820_STATUS_PUBLISHER
  if (this->dump_publisher_ != nullptr)
    this->dump_publisher_(TAG);
#endif
}

BQ25820Config BQ25820Component::load_config_() const {
  BQ25820Config config;
  progmem_memcpy(&config, this->config_, sizeof(config));
  return config;
}

bool BQ25820Component::watchdog_enabled_() const {
  return (progmem_read_byte(&this->config_->controls[CONTROL_TIMER]) & TIMER_CONTROL_WATCHDOG) != 0;
}

// ADC off, charging off, configure, charging on, ADC last: a failure before charger control leaves charging off,
// and any failure leaves ADC_EN clear so the next poll writes it all again.
bool BQ25820Component::write_config_registers_() {
  BQ25820Config config = this->load_config_();
  // The charger control write also feeds the watchdog
  if (this->watchdog_enabled_())
    config.controls[CONTROL_CHARGER] |= WD_RST;
  // Charging goes off even when the ADC write fails
  bool ok = this->update_register(REG_ADC_CONTROL, ADC_EN, 0);
  ok = this->update_register(REG_CHARGER_CONTROL, EN_CHG, 0) && ok;
  for (size_t i = 0; ok && i < BQ25820_LIMIT_COUNT; i++) {
    if (config.limits[i] != 0)
      ok = this->write_byte_16_le(progmem_read_byte(&LIMIT_REGISTERS[i]), config.limits[i]);
  }
  for (size_t i = 0; ok && i < BQ25820_CONTROL_COUNT; i++) {
    ControlRegister entry;
    progmem_memcpy(&entry, &CONTROL_REGISTERS[i], sizeof(entry));
    const uint8_t value = config.controls[i];
    ok = entry.mask == 0xFF ? this->write_byte(entry.reg, value) : this->update_register(entry.reg, entry.mask, value);
  }
  // One panel sweep at startup with charging on, kept pending until it is written
  if (ok && this->force_mppt_sweep_) {
    ok = this->update_register(REG_MPPT_CONTROL, FORCE_SWEEP, FORCE_SWEEP);
    this->force_mppt_sweep_ = !ok;
  }
  // ADC_RATE clear for continuous conversion
  ok = ok && this->update_register(REG_ADC_CONTROL, ADC_EN_RATE_MASK, ADC_EN);
  if (!ok) {
    ESP_LOGW(TAG, "Configuration write failed");
  }
  return ok;
}

bool BQ25820Component::configure_() {
  // The ADC restarts, so the next publish waits for its first conversions
  this->adc_ready_ = false;
  this->rewrite_ = !this->write_config_registers_();
  if (!this->status_comm_result(!this->rewrite_))
    return false;
  this->set_timeout(PUBLISH_TIMEOUT_ID, FIRST_PUBLISH_DELAY_MS, [this]() {
    this->adc_ready_ = true;
    this->update();
  });
  return true;
}

// Every poll also feeds the watchdog, which update_interval is validated to beat
void BQ25820Component::update() {
  BQ25820StatusRegisters status;
  BQ25820AdcRegisters adc;
  bool ok = this->read_bytes(REG_CHARGER_STATUS_1, status.data(), status.size()) &&
#ifdef USE_BQ25820_SENSOR
            this->read_bytes_16_le(REG_ADC_CONTROL, adc.data(), adc.size());
#else
            this->read_byte(REG_ADC_CONTROL, adc.data());
#endif
  // Only the write or the feed below clears the warning, so a failing one is not reported as recovered
  if (!ok) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  const bool watchdog = this->watchdog_enabled_();
  // ADC_EN clear: a partial write or a charger reset; WD_STAT: the watchdog expired
  if (this->rewrite_ || (adc[0] & ADC_EN) == 0 || (watchdog && (status[0] & WD_STAT) != 0)) {
    if (this->adc_ready_) {
      ESP_LOGW(TAG, "Charger lost its configuration, writing it again");
    }
    if (this->configure_())
      return;
  } else {
    // A failed feed still publishes what was read
    this->status_comm_result(
        !watchdog ||
        this->write_byte(REG_CHARGER_CONTROL, progmem_read_byte(&this->config_->controls[CONTROL_CHARGER]) | WD_RST));
  }
#ifdef USE_BQ25820_STATUS_PUBLISHER
  if (this->status_publisher_ != nullptr)
    this->status_publisher_(status);
#endif
#ifdef USE_BQ25820_SENSOR
  if (this->adc_ready_)
    this->publish_adc_(adc);
#endif
}

#ifdef USE_BQ25820_SENSOR
static constexpr float VOLTAGE_ADC_STEP_V = 0.002f;

void BQ25820Component::publish_adc_(const BQ25820AdcRegisters &adc) {
  float iac_scale;
  float ibat_scale;
  progmem_memcpy(&iac_scale, &this->config_->iac_scale, sizeof(iac_scale));
  progmem_memcpy(&ibat_scale, &this->config_->ibat_scale, sizeof(ibat_scale));
  PUBLISH_STATE(this->iac_sensor_, adc[1] * iac_scale);
  // Positive while charging
  PUBLISH_STATE(this->ibat_sensor_, adc[2] * ibat_scale);
  PUBLISH_STATE(this->vac_sensor_, static_cast<uint16_t>(adc[3]) * VOLTAGE_ADC_STEP_V);
  PUBLISH_STATE(this->vbat_sensor_, static_cast<uint16_t>(adc[4]) * VOLTAGE_ADC_STEP_V);
  PUBLISH_STATE(this->vsys_sensor_, static_cast<uint16_t>(adc[5]) * VOLTAGE_ADC_STEP_V);
  PUBLISH_STATE(this->temp_in_c_sensor_, this->ntc_temperature_(adc[6]));
}
#endif

}  // namespace esphome::bq25820
