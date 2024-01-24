#include "ina2xx_base.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cinttypes>
#include <cmath>

namespace esphome {
namespace ina2xx_base {

static const char *const TAG = "ina2xx";

#define OKFAILED(b) ((b) ? "OK" : "FAILED")

void INA2XX::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA2xx...");

  if (!this->reset_config_()) {
    ESP_LOGE(TAG, "Reset failed, check connection");
    this->mark_failed();
    return;
  }
  delay(2);

  if (!this->check_device_type_()) {
    ESP_LOGE(TAG, "Device not supported");
    this->mark_failed();
    return;
  }
  delay(1);

  this->configure_adc_range_();
  delay(1);

  this->configure_adc_();
  delay(1);

  this->configure_shunt_();
  delay(1);

  this->configure_shunt_tempco_();
  delay(1);

  this->state_ = State::IDLE;
}

float INA2XX::get_setup_priority() const { return setup_priority::DATA; }

void INA2XX::update() {
  ESP_LOGD(TAG, "Updating");
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGD(TAG, "Initiating new data collection");
    this->state_ = State::DATA_COLLECTION_1;
    return;
  }
}

void INA2XX::loop() {
  static bool all_ok = true;

  if (this->is_ready()) {
    switch (this->state_) {
      case State::NOT_INITIALIZED:
      case State::IDLE:
        break;

      case State::DATA_COLLECTION_1:
        all_ok = true;

        if (this->shunt_voltage_sensor_ != nullptr) {
          float shunt_voltage{0};
          all_ok &= this->read_shunt_voltage_mv_(shunt_voltage);
          this->shunt_voltage_sensor_->publish_state(shunt_voltage);
        }
        this->state_ = State::DATA_COLLECTION_2;
        break;

      case State::DATA_COLLECTION_2:
        if (this->bus_voltage_sensor_ != nullptr) {
          float bus_voltage{0};
          all_ok &= this->read_bus_voltage_(bus_voltage);
          this->bus_voltage_sensor_->publish_state(bus_voltage);
        }
        this->state_ = State::DATA_COLLECTION_3;
        break;

      case State::DATA_COLLECTION_3:
        if (this->die_temperature_sensor_ != nullptr) {
          float die_temperature{0};
          all_ok &= this->read_die_temp_c_(die_temperature);
          this->die_temperature_sensor_->publish_state(die_temperature);
        }
        this->state_ = State::DATA_COLLECTION_4;
        break;

      case State::DATA_COLLECTION_4:
        if (this->current_sensor_ != nullptr) {
          float current{0};
          all_ok &= this->read_current_a_(current);
          this->current_sensor_->publish_state(current);
        }
        this->state_ = State::DATA_COLLECTION_5;
        break;

      case State::DATA_COLLECTION_5:
        if (this->power_sensor_ != nullptr) {
          float power{0};
          all_ok &= this->read_power_w_(power);
          this->power_sensor_->publish_state(power);
        }
        this->state_ = State::DATA_COLLECTION_6;
        break;

      case State::DATA_COLLECTION_6:
        if (this->ina_type_ == INAType::INA_228_229) {
          if (this->energy_sensor_ != nullptr || this->charge_sensor_ != nullptr) {
            this->read_diagnostics_and_act_();
          }
          if (this->energy_sensor_ != nullptr) {
            double energy{0};
            all_ok &= this->read_energy_j_(energy);
            this->energy_sensor_->publish_state(energy);
          }
        }
        this->state_ = State::DATA_COLLECTION_7;
        break;

      case State::DATA_COLLECTION_7:
        if (this->ina_type_ == INAType::INA_228_229) {
          if (this->charge_sensor_ != nullptr) {
            double coulombs{0};
            all_ok &= this->read_charge_c_(coulombs);
            this->charge_sensor_->publish_state(coulombs);
          }
        }
        this->state_ = State::DATA_COLLECTION_8;
        break;

      case State::DATA_COLLECTION_8:
        if (all_ok) {
          this->status_clear_warning();
        } else {
          this->status_set_warning();
        }
        this->state_ = State::IDLE;
        break;

      default:
        ESP_LOGW(TAG, "Unknown state of the component, might be due to memory corruption");
        break;
    }
  }
}

void INA2XX::dump_config() {
  ESP_LOGCONFIG(TAG, "INA2xx:");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with INA2xx failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Shunt resistance = %f Ohm", shunt_resistance_ohm_);
  ESP_LOGCONFIG(TAG, "  Max current = %f A", this->max_current_a_);
  ESP_LOGCONFIG(TAG, "  Shunt temp coeff = %d ppm/°C", this->shunt_tempco_ppm_c_);
  ESP_LOGCONFIG(TAG, "  ADCRANGE = %d (%s)", (uint8_t) this->adc_range_, this->adc_range_ ? "±40.96 mV" : "±163.84 mV");
  ESP_LOGCONFIG(TAG, "  CURRENT_LSB = %f", this->current_lsb_);
  ESP_LOGCONFIG(TAG, "  SHUNT_CAL = %d", this->shunt_cal_);

  auto get_device_name = [](INAType typ) {
    switch (typ) {
      case INAType::INA_228_229:
        return "INA228/229";
      case INAType::INA_238_239:
        return "INA238/239";
      case INAType::INA_237:
        return "INA237";
      default:
        return "UNKNOWN";
    }
  };

  ESP_LOGCONFIG(TAG, "  Device is %s", get_device_name(this->ina_type_));

  LOG_SENSOR("  ", "Shunt Voltage", this->shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Die Temperature", this->die_temperature_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "[INA228/229] Energy", this->energy_sensor_);
  LOG_SENSOR("  ", "[INA228/229] Charge", this->charge_sensor_);
}

bool INA2XX::reset_energy_counters() {
  if (this->ina_type_ != INAType::INA_228_229) {
    return false;
  }
  ESP_LOGD(TAG, "reset_energy_counters");

  ConfigurationRegister cfg{0};
  auto ret = this->read_unsigned_16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
  cfg.RSTACC = true;
  cfg.ADCRANGE = this->adc_range_;
  ret = ret && this->write_unsigned_16_(RegisterMap::REG_CONFIG, cfg.raw_u16);

  this->energy_overflows_count_ = 0;
  this->charge_overflows_count_ = 0;
  return ret;
}

bool INA2XX::reset_config_() {
  ESP_LOGD(TAG, "Reset");
  ConfigurationRegister cfg{0};
  cfg.RST = true;
  return this->write_unsigned_16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
}

bool INA2XX::check_device_type_() {
  constexpr uint16_t manufacturer_ti = 0x5449;  // "TI"

  uint16_t manufacturer_id{0}, dev_id{0}, rev_id{0};
  this->read_unsigned_16_(RegisterMap::REG_MANUFACTURER_ID, manufacturer_id);
  if (!this->read_unsigned_16_(RegisterMap::REG_DEVICE_ID, dev_id)) {
    dev_id = 0;
    ESP_LOGD(TAG, "Can't read device ID");
  };
  rev_id = dev_id & 0x0F;
  dev_id >>= 4;
  ESP_LOGI(TAG, "Manufacturer: 0x%04X, Device ID: 0x%04X, Revision: %d", manufacturer_id, dev_id, rev_id);

  if (manufacturer_id != manufacturer_ti) {
    ESP_LOGE(TAG, "Manufacturer ID doesn't match original 0x5449");
    return false;
  }

  if (dev_id == 0x228 || dev_id == 0x229) {
    this->ina_type_ = INAType::INA_228_229;
    ESP_LOGI(TAG, "Supported device found: INA%x, 85-V, 20-Bit, Ultra-Precise Power/Energy/Charge Monitor", dev_id);
  } else if (dev_id == 0x238 || dev_id == 0x239) {
    this->ina_type_ = INAType::INA_238_239;
    ESP_LOGI(TAG, "Supported device found: INA%x, 85-V, 16-Bit, High-Precision Power Monitor", dev_id);
  } else if (dev_id == 0x0 || dev_id == 0xFF) {
    ESP_LOGI(TAG, "We assume device is: INA237 85-V, 16-Bit, Precision Power Monitor");
    this->ina_type_ = INAType::INA_237;
  } else {
    ESP_LOGE(TAG, "Unknown device ID %x. Please do not hesitate to report to author.", dev_id);
    return false;
  }

  // setup device coefficients
  if (this->ina_type_ == INAType::INA_228_229) {
    this->cfg_.vbus_lsb = 0.0001953125f;
    this->cfg_.v_shunt_lsb_range0 = 0.0003125f;
    this->cfg_.v_shunt_lsb_range1 = 0.000078125f;
    this->cfg_.shunt_cal_scale = 13107.2f * 1000000.0f;
    this->cfg_.current_lsb_scale_factor = -19;
    this->cfg_.die_temp_lsb = 0.0078125f;
    this->cfg_.power_coeff = 3.2f;
    this->cfg_.energy_coeff = 16.0f * 3.2f;
  } else {
    this->cfg_.vbus_lsb = 0.0031250000f;
    this->cfg_.v_shunt_lsb_range0 = 0.0050000f;
    this->cfg_.v_shunt_lsb_range1 = 0.001250000f;
    this->cfg_.shunt_cal_scale = 819.2f * 1000000.0f;
    this->cfg_.current_lsb_scale_factor = -15;
    this->cfg_.die_temp_lsb = 0.1250000f;
    this->cfg_.power_coeff = 0.2f;
    this->cfg_.energy_coeff = 0.0f;  // N/A
  }

  return true;
}

bool INA2XX::configure_adc_range_() {
  ESP_LOGD(TAG, "Setting ADCRANGE = %d", (uint8_t) this->adc_range_);
  ConfigurationRegister cfg{0};
  auto ret = this->read_unsigned_16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
  cfg.ADCRANGE = this->adc_range_;
  ret = ret && this->write_unsigned_16_(RegisterMap::REG_CONFIG, cfg.raw_u16);

  return ret;
}

bool INA2XX::configure_adc_() {
  bool ret{false};
  AdcConfigurationRegister adc_cfg{0};
  adc_cfg.MODE = 0x0F;  // Fh = Continuous bus voltage, shunt voltage and temperature
  adc_cfg.VBUSCT = AdcSpeed::ADC_SPEED_1052US;
  adc_cfg.VSHCT = AdcSpeed::ADC_SPEED_1052US;
  adc_cfg.VTCT = AdcSpeed::ADC_SPEED_1052US;
  adc_cfg.AVG = AdcSample::ADC_SAMPLE_1;
  ret = this->write_unsigned_16_(RegisterMap::REG_ADC_CONFIG, adc_cfg.raw_u16);
  return ret;
}

bool INA2XX::configure_shunt_() {
  this->current_lsb_ = ldexp(this->max_current_a_, this->cfg_.current_lsb_scale_factor);
  this->shunt_cal_ = (uint16_t) (cfg_.shunt_cal_scale * this->current_lsb_ * this->shunt_resistance_ohm_);
  if (this->adc_range_)
    this->shunt_cal_ *= 4;

  if (this->shunt_cal_ & 0x8000) {
    // cant be more than 15 bits
    ESP_LOGW(TAG, "Shunt value too high");
  }
  this->shunt_cal_ &= 0x7FFF;
  ESP_LOGD(TAG, "Given Rshunt=%f Ohm and Max_current=%.3f", shunt_resistance_ohm_, max_current_a_);
  ESP_LOGD(TAG, "New CURRENT_LSB=%f, SHUNT_CAL=%u", this->current_lsb_, this->shunt_cal_);
  return this->write_unsigned_16_(RegisterMap::REG_SHUNT_CAL, this->shunt_cal_);
}

bool INA2XX::configure_shunt_tempco_() {
  // Only for 228/229
  // unsigned 14-bit value
  // 0x0000 = 0 ppm/°C
  // 0x3FFF = 16383 ppm/°C
  if (this->ina_type_ == INAType::INA_228_229 && this->shunt_tempco_ppm_c_ > 0) {
    return this->write_unsigned_16_(RegisterMap::REG_SHUNT_TEMPCO, this->shunt_tempco_ppm_c_ & 0x3FFF);
  }
  return true;
}

bool INA2XX::read_shunt_voltage_mv_(float &volt_out) {
  // Two's complement value
  //      228, 229 - 24bit: 20(23-4) + 4(3-0) res
  // 237, 238, 239 - 16bit

  bool ret{false};
  float volt_reading{0};

  if (this->ina_type_ == INAType::INA_228_229) {
    ret = this->read_signed_20_4_(RegisterMap::REG_VSHUNT, volt_reading);
  } else {
    ret = this->read_signed_16_(RegisterMap::REG_VSHUNT, volt_reading);
  }

  if (ret)
    volt_out = (this->adc_range_ ? this->cfg_.v_shunt_lsb_range1 : this->cfg_.v_shunt_lsb_range0) * volt_reading;  // mV

  ESP_LOGD(TAG, "read_shunt_voltage_mv_ ret=%s, reading_lsb=%f", OKFAILED(ret), volt_reading);

  return ret;
}

bool INA2XX::read_bus_voltage_(float &volt_out) {
  // Two's complement value
  //      228, 229 - 24bit: 20(23-4) + 4(3-0) res
  // 237, 238, 239 - 16bit

  bool ret{false};
  float volt_reading{0};

  if (this->ina_type_ == INAType::INA_228_229) {
    ret = this->read_signed_20_4_(RegisterMap::REG_VBUS, volt_reading);
  } else {
    ret = this->read_signed_16_(RegisterMap::REG_VBUS, volt_reading);
  }
  if (ret)
    volt_out = this->cfg_.vbus_lsb * (float) volt_reading;

  ESP_LOGD(TAG, "read_bus_voltage_ ret=%s, reading_lsb=%f", OKFAILED(ret), volt_reading);
  return ret;
}

bool INA2XX::read_die_temp_c_(float &temp_out) {
  // Two's complement value
  //      228, 229 - 16bit
  // 237, 238, 239 - 16bit: 12(15-4) + 4(3-0) res

  bool ret{false};
  float temp_reading{0};

  if (this->ina_type_ == INAType::INA_228_229) {
    ret = this->read_signed_16_(RegisterMap::REG_DIETEMP, temp_reading);
  } else {
    ret = this->read_signed_12_4_(RegisterMap::REG_DIETEMP, temp_reading);
  }
  if (ret)
    temp_out = this->cfg_.die_temp_lsb * (float) temp_reading;

  ESP_LOGD(TAG, "read_die_temp_c_ ret=%s, reading_lsb=%f", OKFAILED(ret), temp_reading);
  return ret;
}

bool INA2XX::read_current_a_(float &amps_out) {
  // Two's complement value
  //      228, 229 - 24bit: 20(23-4) + 4(3-0) res
  // 237, 238, 239 - 16bit
  bool ret{false};
  float amps_reading{0};
  if (this->ina_type_ == INAType::INA_228_229) {
    ret = this->read_signed_20_4_(RegisterMap::REG_CURRENT, amps_reading);
  } else {
    ret = this->read_signed_16_(RegisterMap::REG_CURRENT, amps_reading);
  }
  ESP_LOGD(TAG, "read_current_a_ ret=%s. current_lsb=%f. reading_lsb=%f", OKFAILED(ret), this->current_lsb_,
           amps_reading);
  if (ret)
    amps_out = this->current_lsb_ * (float) amps_reading;

  return ret;
}

bool INA2XX::read_power_w_(float &power_out) {
  // Unsigned value
  //      228, 229 - 24bit
  // 237, 238, 239 - 24bit
  uint64_t power_reading{0};
  auto ret = this->read_unsigned_((uint8_t) RegisterMap::REG_POWER, 3, power_reading);

  ESP_LOGD(TAG, "read_power_w_ ret=%s, reading_lsb=%d", OKFAILED(ret), (uint32_t) power_reading);
  if (ret)
    power_out = this->cfg_.power_coeff * this->current_lsb_ * (float) power_reading;

  return ret;
}

bool INA2XX::read_energy_j_(double &joules_out) {
  // Unsigned value
  //      228, 229 - 40bit
  // 237, 238, 239 - not available
  if (this->ina_type_ != INAType::INA_228_229) {
    joules_out = 0;
    return false;
  }
  uint64_t joules_reading = 0;
  uint64_t previous_energy = this->energy_overflows_count_ * (((uint64_t) 1) << 40);
  auto ret = this->read_unsigned_((uint8_t) RegisterMap::REG_ENERGY, 5, joules_reading);

  ESP_LOGD(TAG, "read_energy_j_ ret=%s, reading_lsb=0x%" PRIX64 ", current_lsb=%f, overflow_cnt=%d", OKFAILED(ret),
           joules_reading, this->current_lsb_, this->energy_overflows_count_);
  if (ret)
    joules_out = this->cfg_.energy_coeff * this->current_lsb_ * (double) joules_reading + (double) previous_energy;

  return ret;
}

bool INA2XX::read_charge_c_(double &coulombs_out) {
  // Two's complement value
  //      228, 229 - 40bit
  // 237, 238, 239 - not available
  if (this->ina_type_ != INAType::INA_228_229) {
    coulombs_out = 0;
    return false;
  }

  // and what to do with this? datasheet doesnt tell us what if charge is negative
  uint64_t previous_charge = this->charge_overflows_count_ * (((uint64_t) 1) << 39);
  double coulombs_reading = 0;
  auto ret = this->read_signed_40_((uint8_t) RegisterMap::REG_CHARGE, coulombs_reading);

  ESP_LOGD(TAG, "read_charge_c_ ret=%d, curr_charge=%f + 39-bit overflow_cnt=%d", ret, coulombs_reading,
           this->charge_overflows_count_);
  if (ret)
    coulombs_out = this->current_lsb_ * (double) coulombs_reading + (double) previous_charge;

  return ret;
}

bool INA2XX::read_diagnostics_and_act_() {
  if (this->ina_type_ != INAType::INA_228_229) {
    return false;
  }

  DiagnosticRegister diag{0};
  auto ret = this->read_unsigned_16_(RegisterMap::REG_DIAG_ALRT, diag.raw_u16);
  ESP_LOGD(TAG, "read_diagnostics_and_act_ ret=%s, 0x%04X", OKFAILED(ret), diag.raw_u16);

  if (diag.ENERGYOF)
    this->energy_overflows_count_++;  // 40-bit overflow

  if (diag.CHARGEOF)
    this->charge_overflows_count_++;  // 39-bit overflow

  return ret;
}

bool INA2XX::write_unsigned_16_(uint8_t reg, uint16_t val) {
  uint16_t data_out = byteswap(val);
  auto ret = this->write_ina_register(reg, (uint8_t *) &data_out, 2);
  if (!ret) {
    ESP_LOGD(TAG, "write_unsigned_16_ FAILED reg=0x%02X, val=0x%04X", reg, val);
  }
  return ret;
}

bool INA2XX::read_unsigned_(uint8_t reg, uint8_t reg_size, uint64_t &data_out) {
  static uint8_t rx_buf[5] = {0};  // max buffer size

  if (reg_size > 5)
    return false;

  auto ret = this->read_ina_register(reg, rx_buf, reg_size);

  // Combine bytes
  data_out = rx_buf[0];
  for (uint8_t i = 1; i < reg_size; i++) {
    data_out = (data_out << 8) | rx_buf[i];
  }
  ESP_LOGD(TAG, "read_unsigned_ reg=0x%02X, ret=%s, len=%d, val=0x%" PRIX64, reg, OKFAILED(ret), reg_size, data_out);

  return ret;
}

bool INA2XX::read_unsigned_16_(uint8_t reg, uint16_t &out) {
  uint16_t data_in{0};
  auto ret = this->read_ina_register(reg, (uint8_t *) &data_in, 2);
  out = byteswap(data_in);
  ESP_LOGD(TAG, "read_unsigned_16_ 0x%02X, ret= %s, val=0x%04X", reg, OKFAILED(ret), out);
  return ret;
}

bool INA2XX::read_signed_40_(uint8_t reg, double &out) {
  uint64_t value = 0;
  auto ret = this->read_unsigned_((uint8_t) RegisterMap::REG_CHARGE, 5, value);
  // Convert for 2's compliment and signed value
  if (value > 0x7FFFFFFFFFULL) {
    out = (double) (value - 0x10000000000ULL);
  } else {
    out = (double) value;
  }
  return ret;
}

bool INA2XX::read_signed_20_4_(uint8_t reg, float &out) {
  uint64_t value = 0;
  auto ret = this->read_unsigned_((uint8_t) RegisterMap::REG_CHARGE, 3, value);
  // Remove reserved bits
  value = value >> 4;
  // Convert for 2's compliment and signed value
  if (value > 0x7FFFFULL) {
    out = (float) (value - 0x100000ULL);
  } else {
    out = (float) value;
  }
  return ret;
}

bool INA2XX::read_signed_16_(uint8_t reg, float &out) {
  uint16_t value = 0;
  auto ret = this->read_unsigned_16_((uint8_t) RegisterMap::REG_CHARGE, value);
  // Convert for 2's compliment and signed value
  if (value > 0x7FFFU) {
    out = (float) (value - 0x10000U);
  } else {
    out = (float) value;
  }
  return ret;
}

bool INA2XX::read_signed_12_4_(uint8_t reg, float &out) {
  uint16_t value = 0;
  auto ret = this->read_unsigned_16_((uint8_t) RegisterMap::REG_CHARGE, value);
  // Remove reserved bits
  value = value >> 4;
  // Convert for 2's compliment and signed value
  if (value > 0x7FFU) {
    out = (float) (value - 0x1000U);
  } else {
    out = (float) value;
  }
  return ret;
}
}  // namespace ina2xx_base
}  // namespace esphome
