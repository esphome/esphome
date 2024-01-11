#include "veml7700.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace veml7700 {

static const char *const TAG = "veml7700";
static const size_t VEML_REG_SIZE = 2;

float reduce_to_zero(float a, float b) { return (a > b) ? (a - b) : 0; }

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

const char *get_gain_str(Gain gain) {
  static const char *gain_str[GAINS_COUNT] = {"1x", "2x", "1/8x", "1/4x"};
  return gain_str[gain & 0b11];
}

void VEML7700Component::apply_lux_calculation_(Readings &data) {
  static const float MAX_GAIN = 2.0f;
  static const float MAX_ITIME_MS = 800.0f;
  static const float MAX_LX_RESOLUTION = 0.0036f;
  float lux_resolution = (MAX_ITIME_MS / (float) get_itime_ms(data.actual_time)) *
                         (MAX_GAIN / get_gain_coeff(data.actual_gain)) * MAX_LX_RESOLUTION;
  ESP_LOGD(TAG, "Lux resolution for (%d, %s) = %.4f ", get_itime_ms(data.actual_time), get_gain_str(data.actual_gain),
           lux_resolution);

  data.als_lux = lux_resolution * (float) data.als_counts;
  data.white_lux = lux_resolution * (float) data.white_counts;
  data.fake_infrared_lux = reduce_to_zero(data.white_lux, data.als_lux);

  ESP_LOGD(TAG, "%s mode - ALS = %.1f lx, WHITE = %.1f lx, FAKE_IR = %.1f lx",
           this->automatic_mode_enabled_ ? "Automatic" : "Manual", data.als_lux, data.white_lux,
           data.fake_infrared_lux);
}

void VEML7700Component::apply_lux_compensation_(Readings &data) {
  if (!this->lux_compensation_enabled_)
    return;
  auto &local_data = data;
  // Do correction for G1/4 and G1/8
  // Other Gains G1 and G2 are not supposed to be used for lux > 1000,
  // corrections may help, but not a lot.
  //
  // "Illumination values higher than 1000 lx show non-linearity.
  // This non-linearity is the same for all sensors, so a compensation formula can be applied
  // if this light level is exceeded"
  auto compensate = [&local_data](float &lux) {
    auto calculate_high_lux_compensation = [](float lux_veml) -> float {
      return (((6.0135e-13 * lux_veml - 9.3924e-9) * lux_veml + 8.1488e-5) * lux_veml + 1.0023) * lux_veml;
    };

    if (lux > 1000.0f || local_data.actual_gain == Gain::X_1_8 || local_data.actual_gain == Gain::X_1_4) {
      lux = calculate_high_lux_compensation(lux);
    }
  };

  compensate(data.als_lux);
  compensate(data.white_lux);
  data.fake_infrared_lux = reduce_to_zero(data.white_lux, data.als_lux);

  ESP_LOGD(TAG, "Lux compensation - ALS = %.1f lx, WHITE = %.1f lx, FAKE_IR = %.1f lx", data.als_lux, data.white_lux,
           data.fake_infrared_lux);
}

void VEML7700Component::apply_glass_attenuation_(Readings &data) {
  data.als_lux *= this->attenuation_factor_;
  data.white_lux *= this->attenuation_factor_;
  data.fake_infrared_lux = reduce_to_zero(data.white_lux, data.als_lux);
  ESP_LOGD(TAG, "Glass attenuation - ALS = %.1f lx, WHITE = %.1f lx, FAKE_IR = %.1f lx", data.als_lux, data.white_lux,
           data.fake_infrared_lux);
}

ErrorCode VEML7700Component::read_sensor_output_(Readings &data) {
  auto als_err = this->read_register(CommandRegisters::CR_ALS, (uint8_t *) &data.als_counts, VEML_REG_SIZE, false);
  if (als_err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading ALS register, err = %d", als_err);
  }
  auto white_err =
      this->read_register(CommandRegisters::CR_WHITE, (uint8_t *) &data.white_counts, VEML_REG_SIZE, false);
  if (white_err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading WHITE register, err = %d", white_err);
  }

  ConfigurationRegister conf{0};
  auto err = this->read_register(CommandRegisters::CR_ALS_CONF_0, (uint8_t *) conf.raw_bytes, VEML_REG_SIZE, false);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading ALS_CONF_0 register, err = %d", white_err);
  }
  data.actual_gain = conf.ALS_GAIN;
  data.actual_time = conf.ALS_IT;

  ESP_LOGD(TAG, "read_device_counts: ALS = %d, WHITE = %d, Gain = %s, Time = %d ms", data.als_counts, data.white_counts,
           get_gain_str(data.actual_gain), get_itime_ms(data.actual_time));
  return std::max(als_err, white_err);
}

void VEML7700Component::read_data_() {
  ESP_LOGD(TAG, "Reading data");

  Readings data;

  auto err = this->automatic_mode_enabled_ ? this->read_data_automatic_(data) : this->read_data_manual_(data);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }

  this->apply_lux_calculation_(data);
  this->apply_lux_compensation_(data);
  this->apply_glass_attenuation_(data);

  if (this->ambient_light_sensor_ != nullptr) {
    this->ambient_light_sensor_->publish_state(data.als_lux);
  }
  if (this->ambient_light_counts_sensor_ != nullptr) {
    this->ambient_light_counts_sensor_->publish_state(data.als_counts);
  }
  if (this->white_sensor_ != nullptr) {
    this->white_sensor_->publish_state(data.white_lux);
  }
  if (this->white_counts_sensor_ != nullptr) {
    this->white_counts_sensor_->publish_state(data.white_counts);
  }
  if (this->fake_infrared_sensor_ != nullptr) {
    this->fake_infrared_sensor_->publish_state(data.fake_infrared_lux);
  }
  if (this->actual_gain_sensor_ != nullptr) {
    this->actual_gain_sensor_->publish_state(get_gain_coeff(data.actual_gain));
  }
  if (this->actual_integration_time_sensor_ != nullptr) {
    this->actual_integration_time_sensor_->publish_state(get_itime_ms(data.actual_time));
  }
}

ErrorCode VEML7700Component::read_data_manual_(Readings &data) {
  ESP_LOGD(TAG, "read_data_manual_()");

  auto err = this->read_sensor_output_(data);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read counts");
  }
  return err;
}

ErrorCode VEML7700Component::read_data_automatic_(Readings &data) {
  ESP_LOGD(TAG, "read_data_automatic_()");

  IntegrationTime times[INTEGRATION_TIMES_COUNT] = {INTEGRATION_TIME_25MS,  INTEGRATION_TIME_50MS,
                                                    INTEGRATION_TIME_100MS, INTEGRATION_TIME_200MS,
                                                    INTEGRATION_TIME_400MS, INTEGRATION_TIME_800MS};
  Gain gains[GAINS_COUNT] = {X_1_8, X_1_4, X_1, X_2};

  uint8_t time_idx = 2;  // start from 100ms
  uint8_t gain_idx = 0;  // start from lowest gain of 1/8

  // device already pre-configured in auto-mode in configure_() to 100ms and G 1/8
  // skip reconfiguration at first time
  auto err = this->read_sensor_output_(data);
  if (err != i2c::ERROR_OK) {
    return err;
  }

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

      wait_for_device(times[time_idx], 3);
      err = this->reconfigure_time_and_gain_(times[time_idx], gains[gain_idx]);
      if (err != i2c::ERROR_OK) {
        return err;
      }
      err = this->read_sensor_output_(data);
      if (err != i2c::ERROR_OK) {
        return err;
      }
    }
  } else {
    // G = 1 = 1/8, correction needed for bright scenes, do it later
    while (data.als_counts > 10000 && time_idx > 0) {
      ESP_LOGD(TAG, "read_data_automatic_() - decrease integration time");
      time_idx--;

      wait_for_device(times[time_idx], 3);
      err = this->reconfigure_time_and_gain_(times[time_idx], gains[gain_idx]);
      if (err != i2c::ERROR_OK) {
        return err;
      }
      err = this->read_sensor_output_(data);
      if (err != i2c::ERROR_OK) {
        return err;
      }
    }
  }

  return err;
}

ErrorCode VEML7700Component::reconfigure_time_and_gain_(IntegrationTime time, Gain gain) {
  ESP_LOGD(TAG, "reconfigure_time_and_gain_(%d ms, %s)", get_itime_ms(time), get_gain_str(gain));

  ConfigurationRegister als_conf{0};
  als_conf.raw = 0;

  // 1. Shutdown before changing parameters
  als_conf.ALS_SD = true;
  als_conf.ALS_INT_EN = false;
  als_conf.ALS_PERS = Persistence::PERSISTENCE_1;
  als_conf.ALS_IT = time;
  als_conf.ALS_GAIN = gain;
  auto err = this->write_register(CommandRegisters::CR_ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
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
  wait_for_device(time, 3);  // let it work a bit to capture first data samples

  return err;
}

ErrorCode VEML7700Component::configure_() {
  ESP_LOGD(TAG, "Configure");

  if (this->automatic_mode_enabled_) {
    this->integration_time_ = IntegrationTime::INTEGRATION_TIME_100MS;
    this->gain_ = Gain::X_1_8;
  }

  ConfigurationRegister als_conf{0};
  als_conf.raw = 0;
  als_conf.ALS_SD = true;
  als_conf.ALS_INT_EN = false;
  als_conf.ALS_PERS = Persistence::PERSISTENCE_1;
  als_conf.ALS_IT = this->integration_time_;
  als_conf.ALS_GAIN = this->gain_;
  ESP_LOGD(TAG, "Shutdown before config. ALS_CONF_0 to 0x%04X", als_conf.raw);
  auto err = this->write_register(CommandRegisters::CR_ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to shutdown, I2C error %d", err);
    return err;
  }
  wait_for_device(this->integration_time_, 1);

  als_conf.ALS_SD = false;
  ESP_LOGD(TAG, "Turning on. Setting ALS_CONF_0 to 0x%04X", als_conf.raw);
  err = this->write_register(CommandRegisters::CR_ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to turn on, I2C error %d", err);
    return err;
  }

  PSMRegister psm{0};
  psm.PSM = PSM::PSM_MODE_1;
  psm.PSM_EN = false;
  ESP_LOGD(TAG, "Setting PSM to 0x%04X", psm.raw);
  err = this->write_register(CommandRegisters::CR_PWR_SAVING, psm.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to set PSM, I2C error %d", err);
    return err;
  }
  // 2.5 ms before the first measurement is needed, allowing for the correct start of the signal processor and
  // oscillator.
  wait_for_device(this->integration_time_, 3);  // let it work a bit to capture first data samples
  return err;
}

void VEML7700Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VEML7700/6030...");

  auto err = this->configure_();
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Sensor configuration failed");
    this->mark_failed();
  }
}

void VEML7700Component::dump_config() {
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Automatic gain/time: %s", YESNO(this->automatic_mode_enabled_));
  if (!this->automatic_mode_enabled_) {
    ESP_LOGCONFIG(TAG, "  Gain: %s", get_gain_str(this->gain_));
    ESP_LOGCONFIG(TAG, "  Integration time: %d ms", get_itime_ms(this->integration_time_));
  }
  ESP_LOGCONFIG(TAG, "  Lux compensation: %s", YESNO(this->lux_compensation_enabled_));
  ESP_LOGCONFIG(TAG, "  Attenuation factor: %f", this->attenuation_factor_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "ALS channel lux", this->ambient_light_sensor_);
  LOG_SENSOR("  ", "ALS channel counts", this->ambient_light_counts_sensor_);
  LOG_SENSOR("  ", "WHITE channel lux", this->white_sensor_);
  LOG_SENSOR("  ", "WHITE channel counts", this->white_counts_sensor_);
  LOG_SENSOR("  ", "FAKE_IR channel lux", this->fake_infrared_sensor_);
  LOG_SENSOR("  ", "Actual gain", this->actual_gain_sensor_);
  LOG_SENSOR("  ", "Actual integration time", this->actual_integration_time_sensor_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with I2C VEML7700/6030 failed!");
  }
}

void VEML7700Component::update() {
  ESP_LOGD(TAG, "Updating");
  if (this->is_ready() && !this->reading_data_) {
    this->reading_data_ = true;
    this->read_data_();
    this->reading_data_ = false;
  }
}

}  // namespace veml7700
}  // namespace esphome
