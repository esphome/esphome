#include "bq25820.h"

#include <iterator>

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
static const uint8_t LIMIT_REGISTERS[BQ25820_LIMIT_COUNT] PROGMEM = {
    REG_CHARGE_CURRENT_LIMIT, REG_PRECHARGE_CURRENT_LIMIT,    REG_INPUT_CURRENT_LIMIT,
    REG_INPUT_VOLTAGE_LIMIT,  REG_REVERSE_MODE_VOLTAGE_LIMIT,
};
// The 8-bit registers in BQ25820Config::controls order, and the bits the configuration owns in each
struct ControlRegister {
  uint8_t reg;
  uint8_t mask;
};
static const ControlRegister CONTROL_REGISTERS[BQ25820_CONTROL_COUNT] PROGMEM = {
    {REG_CHARGE_VOLTAGE_LIMIT, 0x1F},  // VFB_REG
    {REG_TIMER_CONTROL, 0xFE},         // everything but EN_TMR2X
    {REG_POWER_PATH_CONTROL, 0x04},    // EN_BATFET_IDEAL_DIODE
    {REG_MPPT_CONTROL, 0x07},          // FULL_SWEEP_TMR[2:1], EN_MPPT
    {REG_TS_THRESHOLD_CONTROL, 0xFF},  // TS_T1, TS_T2, TS_T3, TS_T5
    {REG_TS_REGION_CONTROL, 0x01},     // EN_TS
    {REG_ADC_CONTROL, 0xC0},           // ADC_EN, ADC_RATE
    {REG_CHARGER_CONTROL, 0xFF},       // VRECHG, EN_HIZ, EN_CHG; last, so charging starts with every limit in place
};
static constexpr size_t CONTROL_TIMER = 1;
static constexpr size_t CONTROL_MPPT = 3;
static constexpr size_t CONTROL_CHARGER = 7;
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

// A current limit step is 250 uV across the sense resistor: 25 IBAT or 62.5 IAC ADC steps
static constexpr float CHARGE_LIMIT_ADC_STEPS = 25.0f;
static constexpr float INPUT_LIMIT_ADC_STEPS = 62.5f;

#ifdef USE_BQ25820_DUMP_STATUS_PUBLISHER
#ifdef USE_BINARY_SENSOR
void dump_status_entity(binary_sensor::BinarySensor *sensor) { LOG_BINARY_SENSOR("  ", "Binary Sensor", sensor); }
#endif
#ifdef USE_TEXT_SENSOR
void dump_status_entity(text_sensor::TextSensor *sensor) { LOG_TEXT_SENSOR("  ", "Text Sensor", sensor); }
#endif
#endif

void BQ25820Component::setup() {
  if (!this->check_chip_id(this, REG_PART_INFORMATION, PART_NUM_BQ25820, PART_NUM_MASK))
    return;

  // Start from the reset values; the bit clears itself
  if (!this->setup_check(this->update_register(REG_POWER_PATH_CONTROL, REG_RST, REG_RST)))
    return;

  // Writes the configuration, and retries it on every poll until it succeeds
  this->update();
}

void BQ25820Component::dump_config() {
  const BQ25820Config config = this->load_config_();
  ESP_LOGCONFIG(TAG,
                "BQ25820:\n"
                "  Watchdog: %s\n"
                "  MPPT: %s",
                YESNO(this->watchdog_enabled_()), YESNO(config.controls[CONTROL_MPPT] & EN_MPPT));
  const auto &limits = config.limits;
  if (limits[LIMIT_CHARGE_CURRENT] != 0) {
    ESP_LOGCONFIG(TAG, "  Charge current: %.2f A",
                  (limits[LIMIT_CHARGE_CURRENT] >> 2) * CHARGE_LIMIT_ADC_STEPS * config.ibat_scale);
  }
  if (limits[LIMIT_PRECHARGE_CURRENT] != 0) {
    ESP_LOGCONFIG(TAG, "  Precharge current: %.2f A",
                  (limits[LIMIT_PRECHARGE_CURRENT] >> 2) * CHARGE_LIMIT_ADC_STEPS * config.ibat_scale);
  }
  if (limits[LIMIT_INPUT_CURRENT] != 0) {
    ESP_LOGCONFIG(TAG, "  Input current limit: %.2f A",
                  (limits[LIMIT_INPUT_CURRENT] >> 2) * INPUT_LIMIT_ADC_STEPS * config.iac_scale);
  }
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
#ifdef USE_BQ25820_DUMP_STATUS_PUBLISHER
  if (this->dump_status_publisher_ != nullptr)
    this->dump_status_publisher_();
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

// Called from update() until it succeeds, and again after a watchdog expiry.
bool BQ25820Component::write_config_registers_() {
  const BQ25820Config config = this->load_config_();
  // Stops at the first failure, before the charger control write can enable charging
  bool ok = true;
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
  // One panel sweep at startup, kept pending until it is written
  if (ok && this->force_mppt_sweep_) {
    ok = this->update_register(REG_MPPT_CONTROL, FORCE_SWEEP, FORCE_SWEEP);
    this->force_mppt_sweep_ = !ok;
  }
  if (!ok) {
    ESP_LOGW(TAG, "Configuration write failed");
    // EN_CHG resets to 1, so charging stays off until the configuration is written
    this->update_register(REG_CHARGER_CONTROL, EN_CHG, 0);
  }
  return ok;
}

// Every poll also feeds the watchdog, which update_interval is validated to beat
void BQ25820Component::update() {
  BQ25820StatusRegisters status;
  bool ok = this->read_bytes(REG_CHARGER_STATUS_1, status.data(), status.size());
  // WD_STAT: the watchdog expired and reset the configuration. A failed status read does not hold back the first
  // write, since the chip charges at its reset settings until then.
  const bool configure = !this->config_written_ || (ok && this->watchdog_enabled_() && (status[0] & WD_STAT) != 0);
  if (configure) {
    this->config_written_ = this->write_config_registers_();
    ok = ok && this->config_written_;
  }
  // Also after a rewrite, so a latched WD_STAT clears instead of forcing a rewrite every poll
  if (ok && this->watchdog_enabled_())
    ok = this->write_byte(REG_CHARGER_CONTROL, progmem_read_byte(&this->config_->controls[CONTROL_CHARGER]) | WD_RST);
  // The status was read before the configuration was written, and the ADC was just enabled again, so a
  // configuration poll publishes nothing
  if (!this->status_comm_result(ok) || configure)
    return;

#ifdef USE_BQ25820_STATUS_PUBLISHER
  if (this->status_publisher_ != nullptr)
    this->status_publisher_(status);
#endif
#ifdef USE_BQ25820_SENSOR
  this->publish_adc_(this->load_config_());
#endif
}

#ifdef USE_BQ25820_SENSOR
static constexpr uint8_t REG_IAC_ADC = 0x2D;
static constexpr float VOLTAGE_ADC_STEP_V = 0.002f;

void BQ25820Component::publish_adc_(const BQ25820Config &config) {
  // IAC, IBAT, VAC, VBAT, VSYS and TS ADCs (0x2D-0x38)
  int16_t adc[6];
  if (!this->status_comm_result(this->read_bytes_16_le(REG_IAC_ADC, adc, std::size(adc))))
    return;
  PUBLISH_STATE(this->iac_sensor_, adc[0] * config.iac_scale);
  // Positive while charging
  PUBLISH_STATE(this->ibat_sensor_, adc[1] * config.ibat_scale);
  PUBLISH_STATE(this->vac_sensor_, static_cast<uint16_t>(adc[2]) * VOLTAGE_ADC_STEP_V);
  PUBLISH_STATE(this->vbat_sensor_, static_cast<uint16_t>(adc[3]) * VOLTAGE_ADC_STEP_V);
  PUBLISH_STATE(this->vsys_sensor_, static_cast<uint16_t>(adc[4]) * VOLTAGE_ADC_STEP_V);
  PUBLISH_STATE(this->temp_in_c_sensor_, this->ntc_temperature_(adc[5]));
}
#endif

}  // namespace esphome::bq25820
