#include "npm1300.h"
#include <cmath>
#include "esphome/core/log.h"

namespace esphome::npm1300 {

ESPHOME_LOG_TAG(TAG, "npm1300");

// Registers: the base address of the peripheral in the high byte, the offset in the low byte
static constexpr uint16_t REG_VBUSIN_ILIM0 = 0x0201;
static constexpr uint16_t REG_VBUSIN_STATUS = 0x0207;
static constexpr uint16_t REG_BCHG_ISET_MSB = 0x0308;
static constexpr uint16_t REG_BCHG_ISET_DISCHARGE_MSB = 0x030A;
static constexpr uint16_t REG_BCHG_CHARGE_STATUS = 0x0334;
static constexpr uint16_t REG_BCHG_ERR_REASON = 0x0336;
static constexpr uint16_t REG_ADC_TASK_VBAT = 0x0500;
static constexpr uint16_t REG_ADC_TASK_VBUS = 0x0507;
static constexpr uint16_t REG_ADC_NTC_R_SEL = 0x050A;
static constexpr uint16_t REG_ADC_RESULTS = 0x0510;
static constexpr uint16_t REG_ADC_IBAT_MEAS_EN = 0x0524;

// VBUSINSTATUS
static constexpr uint8_t VBUS_PRESENT = 0x01;
static constexpr uint8_t VBUS_CURRENT_LIMIT_ACTIVE = 0x02;
// BCHGCHARGESTATUS
static constexpr uint8_t CHARGE_STATUS_BATTERY_DETECTED = 0x01;
static constexpr uint8_t CHARGE_STATUS_COMPLETED = 0x02;
static constexpr uint8_t CHARGE_STATUS_CHARGING = 0x1C;  // trickle, constant current or constant voltage
// ADCIBATMEASSTATUS
static constexpr uint8_t IBAT_MEAS_INVALID = 0x10;
static constexpr uint8_t IBAT_MODE_SHIFT = 2;
static constexpr uint8_t IBAT_MODE_CHARGE = 0x03;

// ADC full scale values (product specification, section ADC)
static constexpr float ADC_MAX_CODE = 1023.0f;
static constexpr float VBAT_FULL_SCALE_V = 5.0f;
static constexpr float VSYS_FULL_SCALE_V = 6.375f;
static constexpr float VBUS_FULL_SCALE_V = 7.5f;
static constexpr float CHARGE_FULL_SCALE_FACTOR = 1.25f;
static constexpr float DISCHARGE_FULL_SCALE_FACTOR = 1.12f;
// BCHGISETDISCHARGE code of the 200 mA discharge limit; the other setting is 1000 mA
static constexpr uint16_t DISCHARGE_LIMIT_200MA_CODE = 84;
// Five conversions of 250 us each; the loop runs the timeout later than this anyway
static constexpr uint32_t CONVERSION_TIME_MS = 2;

static constexpr uint32_t READ_TIMEOUT_ID = 0;

void NPM1300Component::setup() {
  uint8_t status;
  if (!this->setup_check(this->read_reg_(REG_BCHG_CHARGE_STATUS, &status)))
    return;
  if (this->thermistor_ != NPM1300_UNSET && !this->setup_check(this->write_reg_(REG_ADC_NTC_R_SEL, this->thermistor_)))
    return;
#ifdef USE_SENSOR
  // The battery current is measured after every battery voltage measurement
  if (!this->setup_check(this->write_reg_(REG_ADC_IBAT_MEAS_EN, 1) && this->read_charge_currents_()))
    return;
#endif
  this->update();
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

void NPM1300Component::update() {
  uint8_t vbus_status;
  uint8_t charge_status;
  uint8_t error_reason;
  if (!this->status_comm_check(this->read_reg_(REG_VBUSIN_STATUS, &vbus_status) &&
                               this->read_reg_(REG_BCHG_CHARGE_STATUS, &charge_status) &&
                               this->read_reg_(REG_BCHG_ERR_REASON, &error_reason)))
    return;
  this->publish_status_(vbus_status, charge_status, error_reason);

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
  if (error_reason != this->last_error_reason_) {
    this->last_error_reason_ = error_reason;
    if (error_reason != 0) {
      ESP_LOGW(TAG, "Charger error, BCHGERRREASON 0x%02X", error_reason);
    }
  }
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->vbus_present_binary_sensor_, (vbus_status & VBUS_PRESENT) != 0);
  PUBLISH_STATE(this->vbus_current_limit_active_binary_sensor_, (vbus_status & VBUS_CURRENT_LIMIT_ACTIVE) != 0);
  PUBLISH_STATE(this->charging_binary_sensor_, (charge_status & CHARGE_STATUS_CHARGING) != 0);
  PUBLISH_STATE(this->charge_complete_binary_sensor_, (charge_status & CHARGE_STATUS_COMPLETED) != 0);
  PUBLISH_STATE(this->battery_detected_binary_sensor_, (charge_status & CHARGE_STATUS_BATTERY_DETECTED) != 0);
  PUBLISH_STATE(this->charger_error_binary_sensor_, error_reason != 0);
#endif
}

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
}

}  // namespace esphome::npm1300
