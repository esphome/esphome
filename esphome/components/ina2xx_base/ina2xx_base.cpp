#include "ina2xx_base.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cinttypes>

namespace esphome {
namespace ina2xx_base {

static const char *const TAG = "ina2xx";

#define OKFAILED(b) ((b) ? "OK" : "FAILED")

static const double VBUS_LSB = 0.0001953125;
static const double V_SHUNT_LSB_RANGE0 = 0.0003125;
static const double V_SHUNT_LSB_RANGE1 = 0.000078125;
static const double DIE_TEMP_LSB = 0.0078125;

uint64_t INA2XX::read_reg(uint8_t reg, uint8_t reg_size) {
  uint64_t value;
  uint8_t rx_buf[5] = {0};  // max buffer size
  auto ret = this->read_2xx(reg, rx_buf, reg_size);

  // Combine bytes
  value = rx_buf[0];
  for (uint8_t i = 1; i < reg_size; i++) {
    value = (value << 8) | rx_buf[i];
  }

  return ret ? 0 : value;
}

bool INA2XX::read_u16_(uint8_t reg, uint16_t &out) {
  uint16_t data_in{0};
  auto ret = this->read_2xx(reg, (uint8_t *) &data_in, 2);
  ESP_LOGD(TAG, "read_u16_ 0x%02X, ret= %d, raw 0x%04X", reg, ret, data_in);
  out = byteswap(data_in);
  return ret;
}

bool INA2XX::read_u24_(uint8_t reg, uint32_t &out) {
  // Two's complement value. Highest bit is the sign
  uint32_t data_in{0};
  auto ret = this->read_2xx(reg, (uint8_t *) &data_in, 3);
  ESP_LOGD(TAG, "read_u24_ 0x%02X, ret= %d, raw 0x%08X", reg, ret, data_in);

  out = byteswap(data_in & 0xFFFFFF) >> 8;
  return ret;
}

bool INA2XX::read_s20_4_(uint8_t reg, int32_t &out) {
  // Two's complement value. Highest bit is the sign
  int32_t data_in{0};
  auto ret = this->read_2xx(reg, (uint8_t *) &data_in, 3);
  // this->read_register(reg, (uint8_t *) &data_in, 3, false);
  ESP_LOGD(TAG, "read_s20_4_ 0x%02X, ret= %d, raw 0x%08X", reg, ret, data_in);

  bool sign = data_in & 0x80;
  data_in = byteswap(data_in & 0xFFFFFF) >> 12;
  if (sign)
    data_in += 0xFFF00000;
  out = data_in;
  return ret;
}

bool INA2XX::write_u16_(uint8_t reg, uint16_t val) {
  uint16_t data_out = byteswap(val);
  auto ret = this->write_2xx(reg, (uint8_t *) &data_out, 2);
  if (!ret) {
    ESP_LOGD(TAG, "write_u16 failed ret=%d, reg=0x%02X, val=0x%04X", ret, reg, val);
  }
  return ret;
}

void INA2XX::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA228...");

  if (!this->reset_config_()) {
    ESP_LOGE(TAG, "Reset failed, check connection");
    this->mark_failed();
    return;
  }
  delay(2);

  uint16_t manufacturer_id{0}, dev_id{0}, rev_id{0};
  this->read_u16_(RegisterMap::REG_MANUFACTURER_ID, manufacturer_id);
  this->read_u16_(RegisterMap::REG_DEVICE_ID, dev_id);
  rev_id = dev_id & 0x0F;
  dev_id >>= 4;
  ESP_LOGI(TAG, "Manufacturer: 0x%04X, Device ID: 0x%04X, Revision: %d", manufacturer_id, dev_id, rev_id);
  delay(1);

  if (manufacturer_id != 0x5449 || dev_id != 0x228) {
    ESP_LOGW(TAG, "Manufacturer ID and device IDs do not match original 0x5449 and 0x228.");
  }
  bool ret = false;

  AdcConfigurationRegister adc_cfg{0};
  ret = this->read_u16_(RegisterMap::REG_ADC_CONFIG, adc_cfg.raw_u16);
  ESP_LOGD(TAG, "Read REG_ADC_CONFIG returned %s, 0x%04X", OKFAILED(ret), adc_cfg.raw_u16);

  adc_cfg.MODE = 0x0F;  // Fh = Continuous bus voltage, shunt voltage and temperature
  adc_cfg.VBUSCT = AdcSpeed::ADC_SPEED_1052US;
  adc_cfg.VSHCT = AdcSpeed::ADC_SPEED_1052US;
  adc_cfg.VTCT = AdcSpeed::ADC_SPEED_1052US;
  adc_cfg.AVG = AdcSample::ADC_SAMPLE_1;
  ret = this->write_u16_(RegisterMap::REG_ADC_CONFIG, adc_cfg.raw_u16);
  ESP_LOGD(TAG, "Write REG_ADC_CONFIG2 returned %s, 0x%04X", OKFAILED(ret), adc_cfg.raw_u16);
  delay(2);

  ret = this->read_u16_(RegisterMap::REG_ADC_CONFIG, adc_cfg.raw_u16);
  ESP_LOGD(TAG, "Checking ... Read REG_ADC_CONFIG returned %s, 0x%04X", OKFAILED(ret), adc_cfg.raw_u16);
  delay(2);

  this->configure_shunt_(this->max_current_a_, this->shunt_resistance_ohm_);
  delay(2);

  this->configure_adc_range_();
  delay(2);
}

bool INA2XX::reset_config_() {
  ESP_LOGD(TAG, "Reset");
  ConfigurationRegister cfg{0};
  cfg.RST = true;
  return this->write_u16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
}

bool INA2XX::reset_energy_counters() {
  ConfigurationRegister cfg{0};
  ESP_LOGD(TAG, "clear_energy_counter_");
  auto ret = this->read_u16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
  cfg.RSTACC = true;
  cfg.ADCRANGE = this->adc_range_;
  ret = ret && this->write_u16_(RegisterMap::REG_CONFIG, cfg.raw_u16);

  this->energy_overflows_count_ = 0;
  this->charge_overflows_count_ = 0;
  return ret;
}

bool INA2XX::configure_shunt_(double max_current, double r_shunt) {
  //  this->current_lsb_ = max_current / (2 << 19);  // 2 power of 19 //
  this->current_lsb_ = ldexpf(max_current, -19);
  this->shunt_cal_ =
      (uint16_t) ((double) (13107.2 * 1000000) * this->current_lsb_ * r_shunt * (this->adc_range_ ? 4 : 1));

  if (this->shunt_cal_ & 0x8000) {
    // cant be more than 15 bits
    ESP_LOGW(TAG, "Shunt value is too high");
  }
  this->shunt_cal_ &= 0x7FFF;
  ESP_LOGD(TAG, "New Rshunt=%f ohm, max current=%.3f", r_shunt, max_current);
  ESP_LOGD(TAG, "New CURRENT_LSB=%f, SHUNT_CAL=%u", this->current_lsb_, this->shunt_cal_);

  return this->write_u16_(RegisterMap::REG_SHUNT_CAL, this->shunt_cal_);
}

bool INA2XX::configure_adc_range_() {
  //  ESP_LOGD(TAG, "Setting ADCRANGE = %d", (uint8_t) this->adc_range_);
  ConfigurationRegister cfg{0};
  auto ret = this->read_u16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
  // ESP_LOGD(TAG, "set_adc_ranged_ %s, read: 0x%04X", OKFAILED(ret), cfg.raw_u16);
  cfg.ADCRANGE = this->adc_range_;

  ret = ret && this->write_u16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
  // ESP_LOGD(TAG, "set_adc_range_ %s, write: 0x%04X", OKFAILED(ret), cfg.raw_u16);

  return ret;
}

bool INA2XX::read_volt_shunt_mv_(double &volt_out) {
  int32_t volt_reading = 0;
  auto ret = this->read_s20_4_(RegisterMap::REG_VSHUNT, volt_reading);
  ESP_LOGD(TAG, "read_volt_shunt_ %s, 0x%08X, adc_range=%d", OKFAILED(ret), volt_reading, this->adc_range_ ? 1 : 0);

  // ±163.84 mV (ADCRANGE = 0) 312.5 nV/LSB   0.0000003125
  // static const double V_SHUNT_LSB_RANGE0 = 0.0003125;

  // ±40.96 mV (ADCRANGE = 1) 78.125 nV/LSB
  // static const double V_SHUNT_LSB_RANGE1 = 0.000078125;
  volt_out = (this->adc_range_ ? V_SHUNT_LSB_RANGE1 : V_SHUNT_LSB_RANGE0) * (double) volt_reading;  // mV

  return ret;
}

bool INA2XX::read_voltage_(double &volt_out) {
  int32_t volt_reading = 0;
  auto ret = this->read_s20_4_(RegisterMap::REG_VBUS, volt_reading);
  ESP_LOGD(TAG, "read_voltage_ %s, 0x%08X", OKFAILED(ret), volt_reading);
  // Range:      0 V to 85 V
  // Resolution: 195.3125 µV/LSB
  //  static const double VBUS_LSB = 0.0001953125; // V

  volt_out = VBUS_LSB * (double) volt_reading;

  return ret;
}

bool INA2XX::read_die_temp_(double &temp) {
  uint16_t temp_reading = 0;
  auto ret = this->read_u16_(RegisterMap::REG_DIETEMP, temp_reading);
  ESP_LOGD(TAG, "read_die_temp_ %s, 0x%04X", OKFAILED(ret), temp_reading);
  temp = DIE_TEMP_LSB * (double) temp_reading;

  return ret;
}

bool INA2XX::read_current_(double &amps_out) {
  int32_t amps_reading = 0;
  auto ret = this->read_s20_4_(RegisterMap::REG_CURRENT, amps_reading);
  ESP_LOGD(TAG, "read_current_ is %s, 0x%08X, current_lsb=%f", TRUEFALSE(ret), amps_reading, this->current_lsb_);
  amps_out = this->current_lsb_ * (double) amps_reading;

  return ret;
}

bool INA2XX::read_power_(double &power_out) {
  uint32_t power_reading = 0;
  auto ret = this->read_2xx((uint8_t) RegisterMap::REG_POWER, (uint8_t *) &power_reading, 3);
  ESP_LOGD(TAG, "read_power_1 ret= %d, 0x%08X", ret, power_reading);

  power_reading = byteswap(power_reading & 0xffffff) >> 8;
  ESP_LOGD(TAG, "read_power_2 ret= %d, 0x%08X", ret, power_reading);
  power_out = 3.2 * this->current_lsb_ * (double) power_reading;

  return ret;
}

static constexpr uint64_t OVF_40BIT = (((uint64_t) 1) << 40);

bool INA2XX::read_energy_(double &joules_out) {
  uint64_t joules_reading = 0;  // Only 40 bits used
  auto ret = this->read_2xx((uint8_t) RegisterMap::REG_ENERGY, (uint8_t *) &joules_reading, 5);
  ESP_LOGD(TAG, "read_energy_1 ret_err= %d, raw 0x%" PRIX64, ret, joules_reading);

  joules_reading = byteswap(joules_reading & 0xffffffffffULL) >> 24;
  ESP_LOGD(TAG, "read_energy_2 ret_err= %d, 0x%" PRIX64 ", current_lsb=%f, overflow_cnt=%d", ret, joules_reading,
           this->current_lsb_, this->energy_overflows_count_);
  uint64_t previous_energy = OVF_40BIT * this->energy_overflows_count_;
  joules_out = 16.0f * 3.2f * this->current_lsb_ * (double) joules_reading + (double) previous_energy;

  return ret;
}

static constexpr uint64_t OVF_39BIT = (((uint64_t) 1) << 39);

bool INA2XX::read_charge_(double &coulombs_out) {
  int64_t coulombs_reading = 0;  // Only 40 bits used
  auto ret = this->read_2xx((uint8_t) RegisterMap::REG_CHARGE, (uint8_t *) &coulombs_reading, 5);
  ESP_LOGD(TAG, "read_charge_1 ret_err= %d, 0x%" PRIX64, ret, coulombs_reading);

  bool sign = coulombs_reading & 0x80;
  coulombs_reading = byteswap(coulombs_reading & 0xffffffffffULL) >> 24;
  if (sign)
    coulombs_reading += 0xFFFFFF0000000000;

  uint64_t previous_charge =
      OVF_39BIT *
      this->charge_overflows_count_;  // and what to do with this? datasheet doesnt tell us what if charge is negative

  ESP_LOGD(TAG, "read_charge_2 ret_err=%d, 0x%" PRIX64 ", current_lsb=%f, overflow_cnt=%d", ret, coulombs_reading,
           this->current_lsb_, this->charge_overflows_count_);
  coulombs_out = this->current_lsb_ * (double) coulombs_reading + (double) previous_charge;

  return ret;
}

bool INA2XX::read_diagnostics_and_act_() {
  DiagnosticRegister diag{0};
  auto ret = this->read_u16_(RegisterMap::REG_DIAG_ALRT, diag.raw_u16);
  ESP_LOGD(TAG, "read_diagnostics_and_act_ %s, 0x%04X", OKFAILED(ret), diag.raw_u16);

  if (diag.ENERGYOF)
    this->energy_overflows_count_++;  // 40-bit overflow

  if (diag.CHARGEOF)
    this->charge_overflows_count_++;  // 40-bit overflow

  return ret;
}

float INA2XX::get_setup_priority() const { return setup_priority::DATA; }

void INA2XX::update() {
  bool all_okay{true};
  if (this->shunt_voltage_sensor_ != nullptr) {
    double shunt_voltage;
    if (!this->read_volt_shunt_mv_(shunt_voltage)) {
      all_okay = false;
      this->status_set_warning();
    }
    this->shunt_voltage_sensor_->publish_state(shunt_voltage);
  }

  if (this->bus_voltage_sensor_ != nullptr) {
    double bus_voltage;
    if (!this->read_voltage_(bus_voltage)) {
      all_okay = false;
      this->status_set_warning();
    }
    this->bus_voltage_sensor_->publish_state(bus_voltage);
  }

  if (this->die_temperature_sensor_ != nullptr) {
    double temp;
    if (!this->read_die_temp_(temp)) {
      all_okay = false;
      this->status_set_warning();
    }
    this->die_temperature_sensor_->publish_state(temp);
  }

  if (this->current_sensor_ != nullptr) {
    double current;
    if (!this->read_current_(current)) {
      all_okay = false;
      this->status_set_warning();
    }
    this->current_sensor_->publish_state(current);
  }

  if (this->power_sensor_ != nullptr) {
    double power;
    if (!this->read_power_(power)) {
      all_okay = false;
      this->status_set_warning();
    }
    this->power_sensor_->publish_state(power);
  }

  if (this->energy_sensor_ != nullptr || this->charge_sensor_ != nullptr) {
    this->read_diagnostics_and_act_();
  }

  if (this->energy_sensor_ != nullptr) {
    double energy;
    if (!this->read_energy_(energy)) {
      all_okay = false;
      this->status_set_warning();
    }
    this->energy_sensor_->publish_state(energy);
  }

  if (this->charge_sensor_ != nullptr) {
    double charge;
    if (!this->read_charge_(charge)) {
      all_okay = false;
      this->status_set_warning();
    }
    this->charge_sensor_->publish_state(charge);
  }

  if (all_okay)
    this->status_clear_warning();
}

}  // namespace ina2xx_base
}  // namespace esphome
