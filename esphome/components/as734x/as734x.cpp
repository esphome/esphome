#include "as734x.h"
#include <cstdio>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include "color_helpers.h"

#ifdef USE_AS7341
#include "as7341.h"
#endif

#ifdef USE_AS7343
#include "as7343.h"
#endif

namespace esphome::as734x {

static const char *const TAG = "as734x";

static constexpr uint32_t DATA_COLLECTION_TIMEOUT_MS = 30 * 1000;  // 30 seconds
static constexpr uint8_t MAX_AUTO_GAIN_TRIES = 10 * 2;

static constexpr float Y_LX_CIE1931_FACTOR = 683.002f;

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f(const char *str, const ChannelValuesFloat &arr) {
  ESP_LOGD(TAG, "%s, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, ", str, arr[0],
           arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f0(const char *str, const ChannelValuesFloat &arr) {
  ESP_LOGD(TAG, "%s, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, ", str, arr[0],
           arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_d(const char *str, const ChannelValuesUint16 &arr) {
  ESP_LOGD(TAG, "%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ", str, arr[0], arr[1], arr[2], arr[3], arr[4],
           arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
// void log_cn_s(const char *TAG, const char *str, const ChannelNames &arr) {
//   ESP_LOGD(TAG, "%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ", str, arr[0], arr[1], arr[2], arr[3],
//   arr[4],
//            arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
// }

void AS734XComponent::setup_model(Model model) {
  ESP_LOGD(TAG, "Setting up model %u", static_cast<uint8_t>(model));
  this->model_ = model;

  switch (this->model_) {
#ifdef USE_AS7341
    case Model::AS7341: {
      this->device_ = new AS7341(this);
    } break;
#endif
#ifdef USE_AS7343
    case Model::AS7343: {
      this->device_ = new AS7343(this);
    } break;
#endif
    default:
      ESP_LOGE(TAG, "Unknown model");
  }

  if (!this->device_) {
    this->mark_failed();
    return;
  }

  this->number_of_channels_ = this->device_->get_number_of_channels();
  this->set_channel_correction(this->device_->get_default_correction());
  this->dark_current_offset_.fill(0.0f);
}

void AS734XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS734x...");
  LOG_I2C_DEVICE(this);

  if (!this->device_->verify_device_id()) {
    ESP_LOGE(TAG, "Invalid chip ID");
    this->mark_failed();
    return;
  }
  this->device_->enable_power(false);
  delay(10);  // wait for power off
  if (!this->device_->enable_power(true)) {
    ESP_LOGE(TAG, "Power on failed!");
    this->mark_failed();
    return;
  }
  delay(10);  // wait for power on

  this->device_->write_default_config();
  this->device_->write_atime(this->atime_);
  this->device_->write_astep(this->astep_);
  this->device_->write_gain(this->gain_);

  this->device_->enable_led(this->led_enabled_);

  this->state_ = State::IDLE;
}

void AS734XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "AS734x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS734x failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_ == Model::AS7341 ? "AS7341" : "AS7343");
  ESP_LOGCONFIG(TAG, "  Gain: %.1f", this->get_gain_multiplier_(this->gain_));
  ESP_LOGCONFIG(TAG, "  ATIME: %u", this->atime_);
  ESP_LOGCONFIG(TAG, "  ASTEP: %u", this->astep_);
  ESP_LOGCONFIG(TAG, "  Glass attenuation factor: %f", this->glass_attenuation_factor_);
}

void AS734XComponent::enable_led(bool enable) {
  this->led_enabled_ = enable;
  this->device_->enable_led(enable);
}

void AS734XComponent::set_dark_current_calibration(const CalibrationParams &vals) {
  std::fill(this->dark_current_offset_.begin(), this->dark_current_offset_.end(), 0.0f);
  std::copy(vals.begin(), vals.end(), this->dark_current_offset_.begin());
}

void AS734XComponent::set_channel_correction(const CalibrationParams &vals) {
  std::fill(this->channel_correction_.begin(), this->channel_correction_.end(), 1.0f);
  std::copy(vals.begin(), vals.end(), this->channel_correction_.begin());

  log_cn_f("    Corr Coeff", this->channel_correction_);
}

void AS734XComponent::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");
    this->state_ = State::START_MEASUREMENT;
  } else {
    ESP_LOGV(TAG, "Can't initiate new data collection - component not ready");
  }
}

void AS734XComponent::loop() {
  if (this->is_ready()) {
    switch (this->state_) {
      case State::NOT_INITIALIZED: {
        // we shall not be here
        ESP_LOGE(TAG, "State machine not initialized");
        this->mark_failed();
      } break;

      case State::IDLE: {
      } break;

      case State::START_MEASUREMENT: {
        ESP_LOGVV(TAG, "START_MEASUREMENT");
        this->readings_.millis_start = millis();

        this->device_->write_atime(this->atime_);
        this->device_->write_astep(this->astep_);
        this->device_->write_gain(this->gain_);

        this->readings_.first_run = true;
        this->readings_.valid = false;
        this->readings_.smux_step = 0;
        this->state_ = State::CONFIGURE_SMUX;
      } break;

      case State::CONFIGURE_SMUX: {
        ESP_LOGVV(TAG, "CONFIGURE_SMUX");
        this->device_->enable_spectral_measurement(false);
        delay(5);
        this->device_->prepare_for_smux_step(this->readings_.smux_step);
        this->state_ = State::WAIT_SMUX;
      } break;

      case State::WAIT_SMUX: {
        ESP_LOGVV(TAG, "WAIT_SMUX");
        if (this->device_->is_smux_ready()) {
          this->device_->enable_spectral_measurement(true);
          this->state_ = State::READ_DATA;
        } else if (millis() - this->readings_.millis_start > DATA_COLLECTION_TIMEOUT_MS) {
          ESP_LOGW(TAG, "SMUX configuration timeout");
          this->state_ = State::IDLE;
        }
      } break;

      case State::READ_DATA: {
        ESP_LOGVV(TAG, "READ_DATA");
        if (this->device_->is_data_ready()) {
          bool device_saturated = false;
          this->device_->read_channels(this->readings_.smux_step, this->readings_.raw_counts, this->readings_.gain,
                                       device_saturated);
          if (this->readings_.first_run) {
            this->readings_.first_run = false;
            ESP_LOGVV(TAG, "Discarding first reading");
            break;
          }
          if (device_saturated) {
            ESP_LOGV(TAG, "Latched data affected by saturation");
          }
          ++this->readings_.smux_step;
          if (this->readings_.smux_step == this->device_->get_number_of_smux_steps()) {
            this->device_->enable_spectral_measurement(false);
            this->readings_.valid = true;
            this->state_ = State::DATA_COLLECTED;
          } else {
            this->readings_.first_run = true;
            this->state_ = State::CONFIGURE_SMUX;
          }
        } else if (millis() - this->readings_.millis_start > DATA_COLLECTION_TIMEOUT_MS) {
          ESP_LOGW(TAG, "Data collection timeout");
          this->state_ = State::IDLE;
        }
      } break;

      case State::DATA_COLLECTED: {
        ESP_LOGVV(TAG, "DATA_COLLECTED");

        this->readings_.gain_x = this->get_gain_multiplier_(this->readings_.gain);
        this->readings_.atime = this->device_->read_atime();
        this->readings_.astep = this->device_->read_astep();
        this->readings_.t_int_us = this->get_t_int_us_(this->readings_.atime, this->readings_.astep);

        this->calculate_basic_counts_();

        uint16_t max_adc = this->get_maximum_spectral_adc_(this->readings_.atime, this->readings_.astep);
        uint16_t highest_adc = this->get_highest_value_(this->readings_.raw_counts);
        this->calculated_values_.saturation_level = (max_adc == 0) ? 0 : 100.0f * highest_adc / (float) max_adc;

        ESP_LOGD(TAG, "Parameters:");
        ESP_LOGD(TAG, "  Gain  =  %.1f", this->readings_.gain_x);
        ESP_LOGD(TAG, "  ATIME = %u ", this->readings_.atime);
        ESP_LOGD(TAG, "  ASTEP = %u ", this->readings_.astep);
        ESP_LOGD(TAG, "  TINT  = %.4f ms", this->readings_.t_int_us / 1000.0f);
        ESP_LOGD(TAG, "Readings:");
        ESP_LOGD(TAG, "  Max ADC: %u", max_adc);
        ESP_LOGD(TAG, "  Highest ADC: %.2f%%", this->calculated_values_.saturation_level);
        log_cn_d("  Counts", this->readings_.raw_counts);
        log_cn_f("    - Corr Dark", this->dark_current_offset_);
        log_cn_f("    * Corr Coeff", this->channel_correction_);
        ESP_LOGD(TAG, "    * Glass attn. %.2f", this->glass_attenuation_factor_);
        log_cn_f("  Basic", this->calculated_values_.basic_counts);

        this->state_ = State::CALCULATE_CIE;
      } break;

      case State::CALCULATE_CIE: {
        ESP_LOGVV(TAG, "CALCULATE_CIE");
        this->calculate_color_(this->calculated_values_.cct, this->calculated_values_.lux_from_xyz);
        this->state_ = State::CALCULATE_SPECTRAL;
      } break;

      case State::CALCULATE_SPECTRAL: {
        ESP_LOGVV(TAG, "CALCULATE_SPECTRAL");
        this->calculate_spectral_(this->calculated_values_.lux_from_irradiance, this->calculated_values_.irradiance_par,
                                  this->calculated_values_.ppfd, this->calculated_values_.irradiance,
                                  this->calculated_values_.irradiance_photopic);
        this->state_ = State::VALIDATE_VALUES;
      } break;

      case State::VALIDATE_VALUES: {
        ESP_LOGVV(TAG, "VALIDATE_VALUES");
        //
        // check for wrong readings due to underexposure or overexposure, replace with 0
        //
        this->calculated_values_.lux_from_irradiance = std::max(this->calculated_values_.lux_from_irradiance, 0.0f);
        this->calculated_values_.irradiance = std::max(this->calculated_values_.irradiance, 0.0f);
        this->calculated_values_.irradiance_photopic = std::max(this->calculated_values_.irradiance_photopic, 0.0f);
        this->calculated_values_.irradiance_par = std::max(this->calculated_values_.irradiance_par, 0.0f);
        this->calculated_values_.ppfd = std::max(this->calculated_values_.ppfd, 0.0f);

        this->calculated_values_.lux_from_xyz = std::max(this->calculated_values_.lux_from_xyz, 0.0f);
        this->calculated_values_.cct = std::max(this->calculated_values_.cct, 0.0f);

        ESP_LOGD(TAG, "Calculated values (glass attenuation = %.2f):", this->glass_attenuation_factor_);
        ESP_LOGD(TAG, "  CIE");
        ESP_LOGD(TAG, "    Color temp(XYZ)     : %f K", this->calculated_values_.cct);
        ESP_LOGD(TAG, "    Illuminance(XYZ)    : %f lx", this->calculated_values_.lux_from_xyz);
        ESP_LOGD(TAG, "  Spectral");
        ESP_LOGD(TAG, "    Irradiance          : %f W/m²", this->calculated_values_.irradiance);
        ESP_LOGD(TAG, "    Irradiance(photopic): %f W/m²", this->calculated_values_.irradiance_photopic);
        ESP_LOGD(TAG, "    PAR                 : %f W/m²", this->calculated_values_.irradiance_par);
        ESP_LOGD(TAG, "    PPFD                : %f µmol/s⋅m²", this->calculated_values_.ppfd);
        ESP_LOGD(TAG, "    Illuminance         : %f lx", this->calculated_values_.lux_from_irradiance);

        this->calculated_values_.cct = std::max(this->calculated_values_.cct, 2865.0f);
        this->calculated_values_.cct = std::min(this->calculated_values_.cct, 10000.0f);

        ESP_LOGD(TAG, "  Clamping values");
        ESP_LOGD(TAG, "    Color temp(XYZ)     : %f K", this->calculated_values_.cct);
        //        ESP_LOGD(TAG, "    Illuminance (XYZ)   : %f lx", this->calculated_values_.lux);
        this->state_ = State::READY_TO_PUBLISH_PART_1;
      } break;

      case State::READY_TO_PUBLISH_PART_1:
        ESP_LOGVV(TAG, "READY_TO_PUBLISH_PART_1");
        this->publish_channel_readings_();
        this->state_ = State::READY_TO_PUBLISH_PART_2;
        break;

      case State::READY_TO_PUBLISH_PART_2:
        ESP_LOGVV(TAG, "READY_TO_PUBLISH_PART_2");
        this->publish_basic_counts_();
        this->state_ = State::READY_TO_PUBLISH_PART_3;
        break;

      case State::READY_TO_PUBLISH_PART_3:
        ESP_LOGVV(TAG, "READY_TO_PUBLISH_PART_3");
        this->publish_derived_readings_();
        this->state_ = State::IDLE;
        break;
    }
  }
}

inline void AS734XComponent::publish_sensor_(sensor::Sensor *sensor, float value) {
  if (sensor != nullptr) {
    sensor->publish_state(value);
  }
}

void AS734XComponent::publish_channel_readings_() {
  for (int i = 0; i < this->device_->get_number_of_channels(); i++) {
    this->publish_sensor_(this->band_counts_sensors_[i], this->readings_.raw_counts[i]);
  }
}

void AS734XComponent::publish_basic_counts_() {
  float inv_max = this->normalization_enabled_ ? 1.0f / this->calculated_values_.max_basic_count : 1.0f;
  for (int i = 0; i < this->device_->get_number_of_channels(); i++) {
    this->publish_sensor_(this->band_basic_counts_sensors_[i], this->calculated_values_.basic_counts[i] * inv_max);
  }
}

void AS734XComponent::publish_derived_readings_() {
  if (this->illuminance_sensor_ != nullptr) {
    this->illuminance_sensor_->publish_state(this->calculated_values_.lux_from_xyz);
  }
  if (this->irradiance_sensor_ != nullptr) {
    this->irradiance_sensor_->publish_state(this->calculated_values_.irradiance);
  }
  if (this->irradiance_photopic_sensor_ != nullptr) {
    this->irradiance_photopic_sensor_->publish_state(this->calculated_values_.irradiance_photopic);
  }
  if (this->irradiance_par_sensor_ != nullptr) {
    this->irradiance_par_sensor_->publish_state(this->calculated_values_.irradiance_par);
  }
  if (this->ppfd_sensor_ != nullptr) {
    this->ppfd_sensor_->publish_state(this->calculated_values_.ppfd);
  }
  if (this->color_temperature_sensor_ != nullptr) {
    this->color_temperature_sensor_->publish_state(this->calculated_values_.cct);
  }
  if (this->saturation_level_sensor_ != nullptr) {
    this->saturation_level_sensor_->publish_state(this->calculated_values_.saturation_level);
  }
  if (this->rgb_hex_sensor_ != nullptr) {
    this->rgb_hex_sensor_->publish_state(this->rgb_hex_str_);
  }
  // if (this->saturated_ != nullptr) {
  //   this->saturated_->publish_state(this->readings_.saturated);
  // }
}

void AS734XComponent::calculate_basic_counts_() {
  this->calculated_values_.basic_counts.fill(0.0f);
  this->calculated_values_.max_basic_count = 0;

  const float inv_exposure = 1.0f / (this->readings_.gain_x * this->readings_.t_int_us);

  for (size_t i = 0; i < this->number_of_channels_; i++) {
    float basic_count = this->readings_.raw_counts[i] * inv_exposure;

    basic_count -= this->dark_current_offset_[i];
    basic_count = std::max(basic_count, 0.0f);
    basic_count *= this->device_->get_gain_correction(i, this->readings_.gain);
    basic_count *= this->channel_correction_[i];
    basic_count *= this->glass_attenuation_factor_;

    this->calculated_values_.basic_counts[i] = basic_count;
    this->calculated_values_.max_basic_count = std::max(this->calculated_values_.max_basic_count, basic_count);
  }
}

void AS734XComponent::calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance,
                                          float &irradiance_photopic) {
  lux = ppfd = par = irradiance = irradiance_photopic = 0.0f;

  for (uint8_t i = 0; i < this->number_of_channels_; i++) {
    float bc = this->calculated_values_.basic_counts[i];
    float e, e_ph, e_par, mol;
    this->device_->get_bc_conversion(i, e, e_ph, e_par, mol);

    irradiance += e * bc;
    irradiance_photopic += e_ph * bc;
    par += e_par * bc;
    ppfd += mol * bc;
  }

  ppfd *= 1e3f;                                     // convert to µmol/s·m²
  lux = irradiance_photopic * Y_LX_CIE1931_FACTOR;  // convert to W/m²
}

void AS734XComponent::calculate_color_(float &cct, float &lux) {
  {
    float tri_xyz[3] = {0, 0, 0};
    for (uint8_t i = 0; i < this->device_->get_number_of_channels(); i++) {
      float tx, ty, tz;
      this->device_->get_xyz_conversion(i, tx, ty, tz);
      tri_xyz[0] += tx * this->calculated_values_.basic_counts[i];
      tri_xyz[1] += ty * this->calculated_values_.basic_counts[i];
      tri_xyz[2] += tz * this->calculated_values_.basic_counts[i];
    }

    tristimulus_to_cct(tri_xyz[0], tri_xyz[1], tri_xyz[2], cct);

    lux = tri_xyz[1] * Y_LX_CIE1931_FACTOR;

    ESP_LOGD(TAG, "  XYZ: %.4f, %.4f, %.4f", tri_xyz[0], tri_xyz[1], tri_xyz[2]);
    ESP_LOGD(TAG, "  CCT: %.2f", cct);

    // Normalize to chromaticity (sum = 1) so the resulting color is independent of
    // brightness; otherwise the sRGB conversion clamps to white under normal light.
    float xyz_sum = tri_xyz[0] + tri_xyz[1] + tri_xyz[2];
    if (xyz_sum > 0.0f) {
      uint8_t r, g, b;
      tristimulus_to_hex(tri_xyz[0] / xyz_sum, tri_xyz[1] / xyz_sum, tri_xyz[2] / xyz_sum, r, g, b);
      ESP_LOGD(TAG, "  RGB: %d, %d, %d", r, g, b);
      snprintf(this->rgb_hex_str_, sizeof(this->rgb_hex_str_), "%02x%02x%02x", r, g, b);
    }
  }
}

inline float AS734XComponent::get_t_int_us_(uint16_t atime, uint16_t astep) {
  return (1.0f + atime) * (1.0f + astep) * 2.78f;
}

float AS734XComponent::get_gain_multiplier_(Gain gain) {
  float gainx = ((uint16_t) 1 << (uint8_t) gain);
  // The AS734x sensor's gain values are represented as powers of 2, but the actual gain multiplier
  // is half of this value. This division by 2 adjusts the calculated gain multiplier accordingly.
  return gainx / 2;
}

template<typename T, size_t N> T AS734XComponent::get_highest_value_(std::array<T, N> &data) {
  T max = data[0];
  for (const auto &v : data) {
    if (v > max) {
      max = v;
    }
  }
  return max;
}

uint16_t AS734XComponent::get_maximum_spectral_adc_(uint16_t atime, uint16_t astep) {
  static constexpr uint16_t MAX_ADC_COUNT = 65535;
  uint32_t value = (atime + 1) * (astep + 1);
  if (value > MAX_ADC_COUNT) {
    value = MAX_ADC_COUNT;
  }
  return value;
}
}  // namespace esphome::as734x
