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

static const uint16_t ADC_TIMES[8] = {50, 84, 150, 280, 540, 1052, 2074, 4120};
static const uint16_t ADC_SAMPLES[8] = {1, 4, 16, 64, 128, 256, 512, 1024};

static const char *get_device_name(INAModel model) {
  switch (model) {
    case INAModel::INA_228:
      return "INA228";
    case INAModel::INA_229:
      return "INA229";
    case INAModel::INA_238:
      return "INA238";
    case INAModel::INA_239:
      return "INA239";
    case INAModel::INA_237:
      return "INA237";
    default:
      return "UNKNOWN";
  }
};

static bool check_model_and_device_match(INAModel model, uint16_t dev_id) {
  switch (model) {
    case INAModel::INA_228:
      return dev_id == 0x228;
    case INAModel::INA_229:
      return dev_id == 0x229;
    case INAModel::INA_238:
      return dev_id == 0x238;
    case INAModel::INA_239:
      return dev_id == 0x239;
    case INAModel::INA_237:
      return dev_id == 0x237;
    default:
      return false;
  }
}

void INA2XX::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA2xx...");

  if (!this->reset_config_()) {
    ESP_LOGE(TAG, "Reset failed, check connection");
    this->mark_failed();
    return;
  }
  delay(2);

  if (!this->check_device_model_()) {
    ESP_LOGE(TAG, "Device not supported or model selected improperly in yaml file");
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
  if (this->is_ready()) {
    switch (this->state_) {
      case State::NOT_INITIALIZED:
      case State::IDLE:
        break;

      case State::DATA_COLLECTION_1:
        this->full_loop_is_okay_ = true;

        if (this->shunt_voltage_sensor_ != nullptr) {
          float shunt_voltage{0};
          this->full_loop_is_okay_ &= this->read_shunt_voltage_mv_(shunt_voltage);
          this->shunt_voltage_sensor_->publish_state(shunt_voltage);
        }
        this->state_ = State::DATA_COLLECTION_2;
        break;

      case State::DATA_COLLECTION_2:
        if (this->bus_voltage_sensor_ != nullptr) {
          float bus_voltage{0};
          this->full_loop_is_okay_ &= this->read_bus_voltage_(bus_voltage);
          this->bus_voltage_sensor_->publish_state(bus_voltage);
        }
        this->state_ = State::DATA_COLLECTION_3;
        break;

      case State::DATA_COLLECTION_3:
        if (this->die_temperature_sensor_ != nullptr) {
          float die_temperature{0};
          this->full_loop_is_okay_ &= this->read_die_temp_c_(die_temperature);
          this->die_temperature_sensor_->publish_state(die_temperature);
        }
        this->state_ = State::DATA_COLLECTION_4;
        break;

      case State::DATA_COLLECTION_4:
        if (this->current_sensor_ != nullptr) {
          float current{0};
          this->full_loop_is_okay_ &= this->read_current_a_(current);
          this->current_sensor_->publish_state(current);
        }
        this->state_ = State::DATA_COLLECTION_5;
        break;

      case State::DATA_COLLECTION_5:
        if (this->power_sensor_ != nullptr) {
          float power{0};
          this->full_loop_is_okay_ &= this->read_power_w_(power);
          this->power_sensor_->publish_state(power);
        }
        this->state_ = State::DATA_COLLECTION_6;
        break;

      case State::DATA_COLLECTION_6:
        if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
          if (this->energy_sensor_j_ != nullptr || this->energy_sensor_wh_ != nullptr ||
              this->charge_sensor_c_ != nullptr || this->charge_sensor_ah_ != nullptr) {
            this->read_diagnostics_and_act_();
          }
          if (this->energy_sensor_j_ != nullptr || this->energy_sensor_wh_ != nullptr) {
            double energy_j{0}, energy_wh{0};
            this->full_loop_is_okay_ &= this->read_energy_(energy_j, energy_wh);
            if (this->energy_sensor_j_ != nullptr)
              this->energy_sensor_j_->publish_state(energy_j);
            if (this->energy_sensor_wh_ != nullptr)
              this->energy_sensor_wh_->publish_state(energy_wh);
          }
        }
        this->state_ = State::DATA_COLLECTION_7;
        break;

      case State::DATA_COLLECTION_7:
        if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
          if (this->charge_sensor_c_ != nullptr || this->charge_sensor_ah_ != nullptr) {
            double charge_c{0}, charge_ah{0};
            this->full_loop_is_okay_ &= this->read_charge_(charge_c, charge_ah);
            if (this->charge_sensor_c_ != nullptr)
              this->charge_sensor_c_->publish_state(charge_c);
            if (this->charge_sensor_ah_ != nullptr)
              this->charge_sensor_ah_->publish_state(charge_ah);
          }
        }
        this->state_ = State::DATA_COLLECTION_8;
        break;

      case State::DATA_COLLECTION_8:
        if (this->full_loop_is_okay_) {
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
  ESP_LOGCONFIG(TAG, "  Device model = %s", get_device_name(this->ina_model_));

  if (this->device_mismatch_) {
    ESP_LOGE(TAG, "  Device model mismatch. Found device with ID = %x. Please check your configuration.",
             this->dev_id_);
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with INA2xx failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Shunt resistance = %f Ohm", this->shunt_resistance_ohm_);
  ESP_LOGCONFIG(TAG, "  Max current = %f A", this->max_current_a_);
  ESP_LOGCONFIG(TAG, "  Shunt temp coeff = %d ppm/°C", this->shunt_tempco_ppm_c_);
  ESP_LOGCONFIG(TAG, "  ADCRANGE = %d (%s)", (uint8_t) this->adc_range_, this->adc_range_ ? "±40.96 mV" : "±163.84 mV");
  ESP_LOGCONFIG(TAG, "  CURRENT_LSB = %f", this->current_lsb_);
  ESP_LOGCONFIG(TAG, "  SHUNT_CAL = %d", this->shunt_cal_);

  ESP_LOGCONFIG(TAG, "  ADC Samples = %d; ADC times: Bus = %d μs, Shunt = %d μs, Temp = %d μs",
                ADC_SAMPLES[0b111 & (uint8_t) this->adc_avg_samples_],
                ADC_TIMES[0b111 & (uint8_t) this->adc_time_bus_voltage_],
                ADC_TIMES[0b111 & (uint8_t) this->adc_time_shunt_voltage_],
                ADC_TIMES[0b111 & (uint8_t) this->adc_time_die_temperature_]);

  ESP_LOGCONFIG(TAG, "  Device is %s", get_device_name(this->ina_model_));

  LOG_SENSOR("  ", "Shunt Voltage", this->shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Die Temperature", this->die_temperature_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);

  if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
    LOG_SENSOR("  ", "Energy J", this->energy_sensor_j_);
    LOG_SENSOR("  ", "Energy Wh", this->energy_sensor_wh_);
    LOG_SENSOR("  ", "Charge C", this->charge_sensor_c_);
    LOG_SENSOR("  ", "Charge Ah", this->charge_sensor_ah_);
  }
}

bool INA2XX::reset_energy_counters() {
  if (this->ina_model_ != INAModel::INA_228 && this->ina_model_ != INAModel::INA_229) {
    return false;
  }
  ESP_LOGV(TAG, "reset_energy_counters");

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
  ESP_LOGV(TAG, "Reset");
  ConfigurationRegister cfg{0};
  cfg.RST = true;
  return this->write_unsigned_16_(RegisterMap::REG_CONFIG, cfg.raw_u16);
}

bool INA2XX::check_device_model_() {
  constexpr uint16_t manufacturer_ti = 0x5449;  // "TI"

  uint16_t manufacturer_id{0}, rev_id{0};
  this->read_unsigned_16_(RegisterMap::REG_MANUFACTURER_ID, manufacturer_id);
  if (!this->read_unsigned_16_(RegisterMap::REG_DEVICE_ID, this->dev_id_)) {
    this->dev_id_ = 0;
    ESP_LOGV(TAG, "Can't read device ID");
  };
  rev_id = this->dev_id_ & 0x0F;
  this->dev_id_ >>= 4;
  ESP_LOGI(TAG, "Manufacturer: 0x%04X, Device ID: 0x%04X, Revision: %d", manufacturer_id, this->dev_id_, rev_id);

  if (manufacturer_id != manufacturer_ti) {
    ESP_LOGE(TAG, "Manufacturer ID doesn't match original 0x5449");
    this->device_mismatch_ = true;
    return false;
  }

  if (this->dev_id_ == 0x228 || this->dev_id_ == 0x229) {
    ESP_LOGI(TAG, "Supported device found: INA%x, 85-V, 20-Bit, Ultra-Precise Power/Energy/Charge Monitor",
             this->dev_id_);
  } else if (this->dev_id_ == 0x238 || this->dev_id_ == 0x239) {
    ESP_LOGI(TAG, "Supported device found: INA%x, 85-V, 16-Bit, High-Precision Power Monitor", this->dev_id_);
  } else if (this->dev_id_ == 0x0 || this->dev_id_ == 0xFF) {
    ESP_LOGI(TAG, "We assume device is: INA237 85-V, 16-Bit, Precision Power Monitor");
    this->dev_id_ = 0x237;
  } else {
    ESP_LOGE(TAG, "Unknown device ID %x.", this->dev_id_);
    this->device_mismatch_ = true;
    return false;
  }

  // Check user-selected model agains what we have found. Mark as failed if selected model != found model
  if (!check_model_and_device_match(this->ina_model_, this->dev_id_)) {
    ESP_LOGE(TAG, "Selected model %s doesn't match found device INA%x", get_device_name(this->ina_model_),
             this->dev_id_);
    this->device_mismatch_ = true;
    return false;
  }

  // setup device coefficients
  if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
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
  ESP_LOGV(TAG, "Setting ADCRANGE = %d", (uint8_t) this->adc_range_);
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
  adc_cfg.VBUSCT = this->adc_time_bus_voltage_;
  adc_cfg.VSHCT = this->adc_time_shunt_voltage_;
  adc_cfg.VTCT = this->adc_time_die_temperature_;
  adc_cfg.AVG = this->adc_avg_samples_;
  ret = this->write_unsigned_16_(RegisterMap::REG_ADC_CONFIG, adc_cfg.raw_u16);
  return ret;
}

bool INA2XX::configure_shunt_() {
  this->current_lsb_ = ldexp(this->max_current_a_, this->cfg_.current_lsb_scale_factor);
  this->shunt_cal_ = (uint16_t) (this->cfg_.shunt_cal_scale * this->current_lsb_ * this->shunt_resistance_ohm_);
  if (this->adc_range_)
    this->shunt_cal_ *= 4;

  if (this->shunt_cal_ & 0x8000) {
    // cant be more than 15 bits
    ESP_LOGW(TAG, "Shunt value too high");
  }
  this->shunt_cal_ &= 0x7FFF;
  ESP_LOGV(TAG, "Given Rshunt=%f Ohm and Max_current=%.3f", this->shunt_resistance_ohm_, this->max_current_a_);
  ESP_LOGV(TAG, "New CURRENT_LSB=%f, SHUNT_CAL=%u", this->current_lsb_, this->shunt_cal_);
  return this->write_unsigned_16_(RegisterMap::REG_SHUNT_CAL, this->shunt_cal_);
}

bool INA2XX::configure_shunt_tempco_() {
  // Only for 228/229
  // unsigned 14-bit value
  // 0x0000 = 0 ppm/°C
  // 0x3FFF = 16383 ppm/°C
  if ((this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) &&
      this->shunt_tempco_ppm_c_ > 0) {
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
  uint64_t raw{0};
  if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
    ret = this->read_unsigned_(RegisterMap::REG_VSHUNT, 3, raw);
    raw >>= 4;
    volt_reading = this->two_complement_(raw, 20);
  } else {
    ret = this->read_unsigned_(RegisterMap::REG_VSHUNT, 2, raw);
    volt_reading = this->two_complement_(raw, 16);
  }

  if (ret) {
    volt_out = (this->adc_range_ ? this->cfg_.v_shunt_lsb_range1 : this->cfg_.v_shunt_lsb_range0) * volt_reading;
  }

  ESP_LOGV(TAG, "read_shunt_voltage_mv_ ret=%s, shunt_cal=%d, reading_lsb=%f", OKFAILED(ret), this->shunt_cal_,
           volt_reading);

  return ret;
}

bool INA2XX::read_bus_voltage_(float &volt_out) {
  // Two's complement value
  //      228, 229 - 24bit: 20(23-4) + 4(3-0) res
  // 237, 238, 239 - 16bit

  bool ret{false};
  float volt_reading{0};
  uint64_t raw{0};
  if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
    ret = this->read_unsigned_(RegisterMap::REG_VBUS, 3, raw);
    raw >>= 4;
    volt_reading = this->two_complement_(raw, 20);
  } else {
    ret = this->read_unsigned_(RegisterMap::REG_VBUS, 2, raw);
    volt_reading = this->two_complement_(raw, 16);
  }
  if (ret) {
    volt_out = this->cfg_.vbus_lsb * (float) volt_reading;
  }

  ESP_LOGV(TAG, "read_bus_voltage_ ret=%s, reading_lsb=%f", OKFAILED(ret), volt_reading);
  return ret;
}

bool INA2XX::read_die_temp_c_(float &temp_out) {
  // Two's complement value
  //      228, 229 - 16bit
  // 237, 238, 239 - 16bit: 12(15-4) + 4(3-0) res

  bool ret{false};
  float temp_reading{0};
  uint64_t raw{0};

  if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
    ret = this->read_unsigned_(RegisterMap::REG_DIETEMP, 2, raw);
    temp_reading = this->two_complement_(raw, 16);
  } else {
    ret = this->read_unsigned_(RegisterMap::REG_DIETEMP, 2, raw);
    raw >>= 4;
    temp_reading = this->two_complement_(raw, 12);
  }
  if (ret) {
    temp_out = this->cfg_.die_temp_lsb * (float) temp_reading;
  }

  ESP_LOGV(TAG, "read_die_temp_c_ ret=%s, reading_lsb=%f", OKFAILED(ret), temp_reading);
  return ret;
}

bool INA2XX::read_current_a_(float &amps_out) {
  // Two's complement value
  //      228, 229 - 24bit: 20(23-4) + 4(3-0) res
  // 237, 238, 239 - 16bit
  bool ret{false};
  float amps_reading{0};
  uint64_t raw{0};

  if (this->ina_model_ == INAModel::INA_228 || this->ina_model_ == INAModel::INA_229) {
    ret = this->read_unsigned_(RegisterMap::REG_CURRENT, 3, raw);
    raw >>= 4;
    amps_reading = this->two_complement_(raw, 20);
  } else {
    ret = this->read_unsigned_(RegisterMap::REG_CURRENT, 2, raw);
    amps_reading = this->two_complement_(raw, 16);
  }

  ESP_LOGV(TAG, "read_current_a_ ret=%s. current_lsb=%f. reading_lsb=%f", OKFAILED(ret), this->current_lsb_,
           amps_reading);
  if (ret) {
    amps_out = this->current_lsb_ * (float) amps_reading;
  }

  return ret;
}

bool INA2XX::read_power_w_(float &power_out) {
  // Unsigned value
  //      228, 229 - 24bit
  // 237, 238, 239 - 24bit
  uint64_t power_reading{0};
  auto ret = this->read_unsigned_((uint8_t) RegisterMap::REG_POWER, 3, power_reading);

  ESP_LOGV(TAG, "read_power_w_ ret=%s, reading_lsb=%d", OKFAILED(ret), (uint32_t) power_reading);
  if (ret) {
    power_out = this->cfg_.power_coeff * this->current_lsb_ * (float) power_reading;
  }

  return ret;
}

bool INA2XX::read_energy_(double &joules_out, double &watt_hours_out) {
  // Unsigned value
  //      228, 229 - 40bit
  // 237, 238, 239 - not available
  if (this->ina_model_ != INAModel::INA_228 && this->ina_model_ != INAModel::INA_229) {
    joules_out = 0;
    return false;
  }
  uint64_t joules_reading = 0;
  uint64_t previous_energy = this->energy_overflows_count_ * (((uint64_t) 1) << 40);
  auto ret = this->read_unsigned_((uint8_t) RegisterMap::REG_ENERGY, 5, joules_reading);

  ESP_LOGV(TAG, "read_energy_j_ ret=%s, reading_lsb=0x%" PRIX64 ", current_lsb=%f, overflow_cnt=%d", OKFAILED(ret),
           joules_reading, this->current_lsb_, this->energy_overflows_count_);
  if (ret) {
    joules_out = this->cfg_.energy_coeff * this->current_lsb_ * (double) joules_reading + (double) previous_energy;
    watt_hours_out = joules_out / 3600.0;
  }
  return ret;
}

bool INA2XX::read_charge_(double &coulombs_out, double &amp_hours_out) {
  // Two's complement value
  //      228, 229 - 40bit
  // 237, 238, 239 - not available
  if (this->ina_model_ != INAModel::INA_228 && this->ina_model_ != INAModel::INA_229) {
    coulombs_out = 0;
    return false;
  }

  // and what to do with this? datasheet doesnt tell us what if charge is negative
  uint64_t previous_charge = this->charge_overflows_count_ * (((uint64_t) 1) << 39);
  double coulombs_reading = 0;
  uint64_t raw{0};
  auto ret = this->read_unsigned_((uint8_t) RegisterMap::REG_CHARGE, 5, raw);
  coulombs_reading = this->two_complement_(raw, 40);

  ESP_LOGV(TAG, "read_charge_c_ ret=%d, curr_charge=%f + 39-bit overflow_cnt=%d", ret, coulombs_reading,
           this->charge_overflows_count_);
  if (ret) {
    coulombs_out = this->current_lsb_ * (double) coulombs_reading + (double) previous_charge;
    amp_hours_out = coulombs_out / 3600.0;
  }
  return ret;
}

bool INA2XX::read_diagnostics_and_act_() {
  if (this->ina_model_ != INAModel::INA_228 && this->ina_model_ != INAModel::INA_229) {
    return false;
  }

  DiagnosticRegister diag{0};
  auto ret = this->read_unsigned_16_(RegisterMap::REG_DIAG_ALRT, diag.raw_u16);
  ESP_LOGV(TAG, "read_diagnostics_and_act_ ret=%s, 0x%04X", OKFAILED(ret), diag.raw_u16);

  if (diag.ENERGYOF) {
    this->energy_overflows_count_++;  // 40-bit overflow
  }

  if (diag.CHARGEOF) {
    this->charge_overflows_count_++;  // 39-bit overflow
  }

  return ret;
}

bool INA2XX::write_unsigned_16_(uint8_t reg, uint16_t val) {
  uint16_t data_out = byteswap(val);
  auto ret = this->write_ina_register(reg, (uint8_t *) &data_out, 2);
  if (!ret) {
    ESP_LOGV(TAG, "write_unsigned_16_ FAILED reg=0x%02X, val=0x%04X", reg, val);
  }
  return ret;
}

bool INA2XX::read_unsigned_(uint8_t reg, uint8_t reg_size, uint64_t &data_out) {
  static uint8_t rx_buf[5] = {0};  // max buffer size

  if (reg_size > 5) {
    return false;
  }

  auto ret = this->read_ina_register(reg, rx_buf, reg_size);

  // Combine bytes
  data_out = rx_buf[0];
  for (uint8_t i = 1; i < reg_size; i++) {
    data_out = (data_out << 8) | rx_buf[i];
  }
  ESP_LOGV(TAG, "read_unsigned_ reg=0x%02X, ret=%s, len=%d, val=0x%" PRIX64, reg, OKFAILED(ret), reg_size, data_out);

  return ret;
}

bool INA2XX::read_unsigned_16_(uint8_t reg, uint16_t &out) {
  uint16_t data_in{0};
  auto ret = this->read_ina_register(reg, (uint8_t *) &data_in, 2);
  out = byteswap(data_in);
  ESP_LOGV(TAG, "read_unsigned_16_ 0x%02X, ret= %s, val=0x%04X", reg, OKFAILED(ret), out);
  return ret;
}

int64_t INA2XX::two_complement_(uint64_t value, uint8_t bits) {
  if (value > (1ULL << (bits - 1))) {
    return (int64_t) (value - (1ULL << bits));
  } else {
    return (int64_t) value;
  }
}
}  // namespace ina2xx_base
}  // namespace esphome
