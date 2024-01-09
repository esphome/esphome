#include "veml7700.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace veml7700 {

static const char *const TAG = "veml7700";

static const size_t VEML_REG_SIZE = 2;

uint16_t get_itime_ms(IntegrationTime time) {
  uint16_t ms = 0;
  switch (time) {
    case INTEGRATION_TIME_100MS:
      ms = 100;
      break;
    case INTEGRATION_TIME_200MS:
      ms = 200;
      break;
    case INTEGRATION_TIME_400MS:
      ms = 400;
      break;
    case INTEGRATION_TIME_800MS:
      ms = 800;
      break;
    case INTEGRATION_TIME_50MS:
      ms = 50;
      break;
    case INTEGRATION_TIME_25MS:
      ms = 25;
      break;
    default:
      ms = 100;
  }
  return ms;
}

void wait_for_device(IntegrationTime time, uint16_t mult) {
  delay(mult * get_itime_ms(time));
  App.feed_wdt();
}

float get_gain_coeff(Gain gain) {
  static const float GAIN_FLOAT[GAINS_COUNT] = {1.0f, 2.0f, 0.125f, 0.25f};
  return GAIN_FLOAT[gain & 0b11];
}

const char *get_gain_str(Gain gain_) {
  static const char *GAIN_STR[GAINS_COUNT] = {"x1", "x2", "x1/8", "x1/4"};
  return GAIN_STR[gain_ & 0b11];
}

float get_lux_resolution_(IntegrationTime time, Gain gain) {
  static const float MAX_GAIN = 2.0f;
  static const float MAX_ITIME_MS = 800.0f;
  static const float MAX_LX_RESOLUTION = 0.0036f;
  float res = (MAX_ITIME_MS / get_itime_ms(time)) * (MAX_GAIN / get_gain_coeff(gain)) * MAX_LX_RESOLUTION;
  ESP_LOGD(TAG, "get_lux_resolution_(%d, %s) = %.4f ", get_itime_ms(time), get_gain_str(gain), res);

  return res;
}

void VEML7700Component::apply_lux_compensation_(Readings &data) {
  if (!this->lux_compensation_enabled_)
    return;

  auto calculate_high_lux_compensation = [](float lux_veml) -> float {
    return (((6.0135e-13 * lux_veml - 9.3924e-9) * lux_veml + 8.1488e-5) * lux_veml + 1.0023) * lux_veml;
  };

  // Do correction for G1/4 and G1/8
  // Other Gains G1 and G2 are not supposed to be used for lux > 1000,
  // corrections may help, but not a lot.
  //
  // "Illumination values higher than 1000 lx show non-linearity.
  // This non-linearity is the same for all sensors, so a compensation formula can be applied
  // if this light level is exceeded"

  if (data.als_lux > 1000.0f || data.actual_gain == Gain::X_1_8 || data.actual_gain == Gain::X_1_4)
    data.als_lux = calculate_high_lux_compensation(data.als_lux);
  if (data.white_lux > 1000.0f || data.actual_gain == Gain::X_1_8 || data.actual_gain == Gain::X_1_4)
    data.white_lux = calculate_high_lux_compensation(data.white_lux);
  ESP_LOGD(TAG, "Lux compensation - ALS = %.1f lux, WHITE = %.1f lux", data.als_lux, data.white_lux);
}

void VEML7700Component::apply_glass_attenuation_(Readings &data) {
  data.als_lux *= this->attenuation_factor_;
  data.white_lux *= this->attenuation_factor_;
  ESP_LOGD(TAG, "Glass attenuation - ALS = %.1f lux, WHITE = %.1f lux", data.als_lux, data.white_lux);
}

ErrorCode VEML7700Component::read_device_counts_(uint16_t &als_counts, uint16_t &white_counts) {
  ErrorCode als_err = this->read_register(CommandRegisters::CR_ALS, (uint8_t *) &als_counts, VEML_REG_SIZE, false);
  if (als_err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading ALS register, err = %d", als_err);
  }
  ErrorCode white_err =
      this->read_register(CommandRegisters::CR_WHITE, (uint8_t *) &white_counts, VEML_REG_SIZE, false);
  if (white_err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading WHITE register, err = %d", white_err);
  }
  ESP_LOGD(TAG, "read_device: ALS = %d, WHITE = %d", als_counts, white_counts);
  return std::max(als_err, white_err);
}

bool VEML7700Component::read_data_() {
  ESP_LOGD(TAG, "Reading data");
  Readings data;
  ErrorCode err = this->automatic_mode_ ? this->read_data_automatic_(data) : this->read_data_manual_(data);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning();
  }
  this->apply_lux_compensation_(data);
  this->apply_glass_attenuation_(data);

  if (this->ambient_light_sensor_ != nullptr)
    this->ambient_light_sensor_->publish_state(data.als_lux);

  if (this->ambient_light_counts_sensor_ != nullptr)
    this->ambient_light_counts_sensor_->publish_state(data.als_counts);

  if (this->white_sensor_ != nullptr)
    this->white_sensor_->publish_state(data.white_lux);

  if (this->white_counts_sensor_ != nullptr)
    this->white_counts_sensor_->publish_state(data.white_counts);

  if (this->actual_gain_sensor_ != nullptr)
    this->actual_gain_sensor_->publish_state(get_gain_coeff(data.actual_gain));

  if (this->actual_integration_time_sensor_ != nullptr)
    this->actual_integration_time_sensor_->publish_state(get_itime_ms(data.actual_time));

  return true;
}

ErrorCode VEML7700Component::read_data_manual_(Readings &data) {
  ESP_LOGD(TAG, "read_data_manual_()");
  ErrorCode err = i2c::ERROR_OK;

  err = this->read_device_counts_(data.als_counts, data.white_counts);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read counts");
    return err;
  }

  float lux_resolution = get_lux_resolution_(this->integration_time_, this->gain_);
  data.als_lux = lux_resolution * (float) data.als_counts;
  data.white_lux = lux_resolution * (float) data.white_counts;
  data.actual_time = this->integration_time_;
  data.actual_gain = this->gain_;

  ESP_LOGD(TAG, "Manual mode - direct lux ALS = %.1f lux, WHITE = %.1f lux", data.als_lux, data.white_lux);

  return err;
}

ErrorCode VEML7700Component::read_data_automatic_(Readings &data) {
  ESP_LOGD(TAG, "read_data_automatic_()");

  ErrorCode err = i2c::ERROR_OK;

  IntegrationTime times[INTEGRATION_TIMES_COUNT] = {INTEGRATION_TIME_25MS,  INTEGRATION_TIME_50MS,
                                                    INTEGRATION_TIME_100MS, INTEGRATION_TIME_200MS,
                                                    INTEGRATION_TIME_400MS, INTEGRATION_TIME_800MS};
  Gain gains[GAINS_COUNT] = {X_1_8, X_1_4, X_1, X_2};

  uint8_t time_idx = 2;  // start from 100ms
  uint8_t gain_idx = 0;  // start from lowest gain of 1/8

  IntegrationTime time_selected = times[time_idx];
  Gain gain_selected = gains[gain_idx];

  bool luxCorrectionRequired = false;  // required for 1/8 + high intensity

  this->reconfigure_time_and_gain_(time_selected, gain_selected);
  this->read_device_counts_(data.als_counts, data.white_counts);

  if (data.als_counts <= 100) {
    ESP_LOGD(TAG, "read_data_automatic_() - counts < 100");
    while (data.als_counts <= 100 && gain_idx <= 3 && time_idx <= 5) {
      if (gain_idx < 3) {
        gain_idx++;  // first, try to increase gain, as per datasheet
      } else if (time_idx < 5) {
        time_idx++;  // second, try to increase integration time
      } else {
        break;  // we've already at the max of gain and integration time
      }
      time_selected = times[time_idx];
      gain_selected = gains[gain_idx];

      wait_for_device(time_selected, 3);
      this->reconfigure_time_and_gain_(time_selected, gain_selected);
      this->read_device_counts_(data.als_counts, data.white_counts);
    }
  } else {
    // G = 1 = 1/8, correction needed for bright scenes, do it later
    while (data.als_counts > 10000 && time_idx > 0) {
      ESP_LOGD(TAG, "read_data_automatic_() - decrease integration time");
      time_selected = times[--time_idx];

      wait_for_device(time_selected, 3);
      this->reconfigure_time_and_gain_(time_selected, gain_selected);
      this->read_device_counts_(data.als_counts, data.white_counts);
    }
  }
  float lux_resolution = get_lux_resolution_(time_selected, gain_selected);
  data.als_lux = lux_resolution * (float) data.als_counts;
  data.white_lux = lux_resolution * (float) data.white_counts;
  data.actual_time = time_selected;
  data.actual_gain = gain_selected;
  ESP_LOGD(TAG, "Automatic mode - direct lux ALS = %.1f lux, WHITE = %.1f lux", data.als_lux, data.white_lux);

  return err;
}

ErrorCode VEML7700Component::reconfigure_time_and_gain_(IntegrationTime time, Gain gain) {
  ESP_LOGD(TAG, "reconfigure_time_and_gain_(%d ms, %s)", get_itime_ms(time), get_gain_str(gain));
  ErrorCode err = i2c::ERROR_OK;

  ConfigurationRegister als_conf{0};
  als_conf.raw = 0;

  // 1. Shutdown before changing parameters
  als_conf.ALS_SD = true;
  als_conf.ALS_INT_EN = false;
  als_conf.ALS_PERS = Persistence::PERSISTENCE_1;
  als_conf.ALS_IT = time;
  als_conf.ALS_GAIN = gain;
  err = this->write_register(CommandRegisters::CR_ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Shutdown failed");
    return err;
  }
  wait_for_device(time, 1);

  // 2. Turn back on
  als_conf.ALS_SD = false;
  err = this->write_register(CommandRegisters::CR_ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Turn on failed");
    return err;
  }
  wait_for_device(time, 3);
  // let it work a bit to capture first data samples

  return err;
}

ErrorCode VEML7700Component::configure_() {
  ESP_LOGD(TAG, "Configure");
  ErrorCode err;

  ConfigurationRegister als_conf{0};
  als_conf.raw = 0;
  als_conf.ALS_SD = false;
  als_conf.ALS_INT_EN = false;
  als_conf.ALS_PERS = Persistence::PERSISTENCE_1;
  als_conf.ALS_IT = this->integration_time_;
  als_conf.ALS_GAIN = this->gain_;
  ESP_LOGD(TAG, "Setting ALS_CONF_0 to 0x%04X", als_conf.raw);
  err = this->write_register(CommandRegisters::CR_ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to set ALS_CONF_0, I2C error %d", err);
    this->status_set_warning();
    return err;
  }

  PSMRegister psm{0};
  psm.PSM = PSM::PSM_MODE_1;
  psm.PSM_EN = false;
  ESP_LOGD(TAG, "Setting PSM to 0x%04X", psm.raw);
  err = this->write_register(CommandRegisters::CR_PWR_SAVING, psm.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to set PSM, I2C error %d", err);
    this->status_set_warning();
    return err;
  }
  // 2.5 ms before the first measurement is needed, allowing for the correct start of the signal processor and
  // oscillator.
  delay(3);
  return err;
}

void VEML7700Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VEML7700/VEML6030");
  auto err = this->configure_();
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Sensor configuration failed");
    this->mark_failed();
  }
}

void VEML7700Component::dump_config() {
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Automatic gain/time mode: %s", ONOFF(this->automatic_mode_));
  if (!this->automatic_mode_) {
    ESP_LOGCONFIG(TAG, "  Gain: %s", get_gain_str(this->gain_));
    ESP_LOGCONFIG(TAG, "  Integration time: %d ms", get_itime_ms(this->integration_time_));
  }
  ESP_LOGCONFIG(TAG, "  Attenuation factor: %f", this->attenuation_factor_);
  LOG_UPDATE_INTERVAL(this);

  if (this->ambient_light_sensor_ != nullptr)
    LOG_SENSOR("  ", "ALS channel lux", this->ambient_light_sensor_);
  if (this->ambient_light_sensor_ != nullptr)
    LOG_SENSOR("  ", "ALS channel counts", this->ambient_light_counts_sensor_);
  if (this->white_sensor_ != nullptr)
    LOG_SENSOR("  ", "WHITE channel lux", this->white_sensor_);
  if (this->white_counts_sensor_ != nullptr)
    LOG_SENSOR("  ", "WHITE channel counts", this->white_counts_sensor_);

  if (this->actual_gain_sensor_ != nullptr)
    LOG_SENSOR("  ", "Actual gain", this->actual_gain_sensor_);
  if (this->actual_integration_time_sensor_ != nullptr)
    LOG_SENSOR("  ", "Actual integration time", this->actual_integration_time_sensor_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with I2C VEML-7700 failed!");
  }
}

void VEML7700Component::update() {
  ESP_LOGD(TAG, "Updating");
  if (this->is_ready() && !this->reading_in_progress_) {
    this->reading_in_progress_ = true;
    this->read_data_();
    this->reading_in_progress_ = false;
  }
}

}  // namespace veml7700
}  // namespace esphome
