#include "as7341.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::as7341 {

static const char *const TAG = "as7341";

static constexpr uint32_t DATA_COLLECTION_TIMEOUT_MS = 30 * 1000;  // 30 seconds

static constexpr std::array<const char *, AS7341_NUM_CHANNELS> CHANNEL_NAME = {
    "F1", "F2", "F3", "F4", "x", "xx", "F5", "F6", "F7", "F8", "Clear", "NIR",
};

static constexpr std::array<uint16_t, AS7341_NUM_CHANNELS> CHANNEL_WAVE_NM = {
    410, 440, 470, 510, 0, 0, 590, 630, 680, 730, 850, 940,
};

static constexpr std::array<float, AS7341_GAIN_COUNT> AS7341_GAIN_CORRECTION = {1.06, 1.05, 1.05, 1.05, 1.02, 1.02,
                                                                                1.01, 1.00, 1.00, 0.99, 0.96};

// 1.  as per Golden Device calibration matrix - convoluted matrices for quick calculations
// 1.1. E fullband irradiance (W/m²)
static const float AS7341_IRRAD_MW_PER_COUNT[AS7341_NUM_CHANNELS] = {8.110691277, 0.988166817, 1.513167954, 0.823071218,
                                                                     0,           0,           0.529502926, 0.35304952,
                                                                     0.255581795, 0.132104073, 2.423065737, 0.63983804};
// 1.2. E PAR band 400-700 nm irradiance (W/m²)
static const float AS7341_IRRAD_PAR_E_MW_PER_COUNT[AS7341_NUM_CHANNELS] = {
    5.936230324, 3.572528495, 2.914113437, 2.452851227, 0,         0,
    1.940656433, 1.533976606, 1.81983825,  1.232942274, -1.102368, -0.033304813};

// 1.3 E photopic irradiance (W/m²) - takes into account photopic response curve CIE1931 V(lambda).
// SUM over 380nm-780nm (SPD(lambda)*V(lambda))xdeltaLambda)
static const float AS7341_IRRAD_PHOTOPIC_MW_PER_COUNT[AS7341_NUM_CHANNELS] = {
    0.005261294, 0.242615252, 0.174482552, 1.479499075,  0.00000000,   0.00000000,
    1.906818555, 1.162058142, 0.462374845, -0.017794572, -0.260730901, -0.022604075};

// 1.4 Lux = E photopic irradiance (W/m²) * 683.002 lm/W
// same matrix, multiply the result

// 1.5 Photosyhtetic photon flux density (PPFD) (µmol/s·m²) 400-700 nm only
// K = 0.008359347      # 1e6/(NA·h·c·1e9)  with NA = 6.022 140 76e23, h = 6.626 070 15e-34, c = 2.997 924 58e8
// ppfd = K * integrate(spd[mask] * λ[mask])
static const float AS7341_PPFD_UMOL_PER_COUNT[AS7341_NUM_CHANNELS] = {
    0.020518273, 0.01352549,  0.011585145, 0.010604986, 0.00000000,   0.00000000,
    0.008890531, 0.007441544, 0.00939359,  0.006890812, -0.004647911, -0.000129106,
};

//    1.232942274, -1.102368, -0.033304813, 0, 0, 0, 0, 2940, 0.262366123, 5.283600695, 0.166979542, 0.095833333};

// static const float AS7341_PPFD_UMOL_PER_COUNT[AS7341_NUM_CHANNELS] = {
//     0.021326768,  -0.000755828, 0.002035903,  0.000339802,  0,           0,
//     -0.000200202, -0.000274651, -0.000413752, -0.000562557, 0.018267892, 0.006056582};

const float AS7341_XYZ_PER_COUNT[3][AS7341_NUM_CHANNELS] = {
    {0.39814, 1.29540, 0.36956, 0.10902, 0, 0, 0.71942, 1.78180, 1.10110, -0.03991, -0.27597, -0.02347},
    {0.01396, 0.16748, 0.23538, 1.42750, 0, 0, 1.88670, 1.14200, 0.46497, -0.02702, -0.24468, -0.01993},
    {1.95010, 6.45490, 2.78010, 0.18501, 0, 0, 0.15325, 0.09539, 0.10563, 0.08866, -0.61140, -0.00938}};

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f(const char *TAG, const char *str, const ChannelValuesFloat &arr) {
  ESP_LOGD(TAG, "%8s, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, ", str, arr[0], arr[1], arr[2],
           arr[3], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_f0(const char *TAG, const char *str, const ChannelValuesFloat &arr) {
  ESP_LOGD(TAG, "%s, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, ", str, arr[0], arr[1], arr[2], arr[3],
           arr[6], arr[7], arr[8], arr[9], arr[10], arr[11]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_d(const char *TAG, const char *str, const ChannelValuesUint16 &arr) {
  ESP_LOGD(TAG, "%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ", str, arr[0], arr[1], arr[2], arr[3], arr[6], arr[7],
           arr[8], arr[9], arr[10], arr[11]);
}

// LOGGING. printing out channel values except for last one which is fake just for the safety of inverse mapping
void log_cn_s(const char *TAG, const char *str, const ChannelValuesString &arr) {
  ESP_LOGD(TAG, "%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ", str, arr[0], arr[1], arr[2], arr[3], arr[6], arr[7],
           arr[8], arr[9], arr[10], arr[11]);
}

void AS7341Component::setup() {
  LOG_I2C_DEVICE(this);

  // Verify device ID
  uint8_t id;
  this->read_byte(AS7341_ID, &id);
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  if ((id & 0xFC) != (AS7341_CHIP_ID << 2)) {
    this->mark_failed();
    return;
  }

  // Power on (enter IDLE state)
  if (!this->enable_power(true)) {
    ESP_LOGE(TAG, "  Power on failed!");
    this->mark_failed();
    return;
  }

  delay(20);

  // Set configuration
  this->write_byte(AS7341_CONFIG, 0x00);
  this->setup_atime(this->atime_);
  this->setup_astep(this->astep_);
  this->setup_gain(this->gain_);

  // As7341Cfg8 cfg8{0};
  // cfg8.fifo_th = 2;
  // cfg8.fd_agc_enable = 0;
  // cfg8.sp_agc_enable = 1;                   // enable AGC
  // this->write_byte(AS7341_CFG8, cfg8.raw);  // enable AGC
  delay(10);

  this->enable_spectral_measurement(true);

  this->state_ = State::IDLE;
}

void AS7341Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS7341:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Gain: %u\n"
                "  ATIME: %u\n"
                "  ASTEP: %u",
                get_gain(), get_atime(), get_astep());

  // LOG_SENSOR("  ", "F1", this->band_counts_f1_sensor_);
  // LOG_SENSOR("  ", "F2", this->band_counts_f2_sensor_);
  // LOG_SENSOR("  ", "F3", this->band_counts_f3_sensor_);
  // LOG_SENSOR("  ", "F4", this->band_counts_f4_sensor_);
  // LOG_SENSOR("  ", "F5", this->band_counts_f5_sensor_);
  // LOG_SENSOR("  ", "F6", this->band_counts_f6_sensor_);
  // LOG_SENSOR("  ", "F7", this->band_counts_f7_sensor_);
  // LOG_SENSOR("  ", "F8", this->band_counts_f8_sensor_);
  // LOG_SENSOR("  ", "NIR", this->band_counts_nir_sensor_);
  // LOG_SENSOR("  ", "Clear", this->band_counts_clear_sensor_);
}

void AS7341Component::set_dark_current_calibration(const CalibrationCoeffs &vals) {
  // Copy first 4 values
  for (int i = 0; i < 4; i++) {
    this->dark_current_offset_[i] = vals[i];
  }

  // Set positions 4 and 5 to 1.0
  this->dark_current_offset_[4] = 0.0f;
  this->dark_current_offset_[5] = 0.0f;

  // Copy remaining values starting at position 6
  for (int i = 4; i < AS7341_NUM_CHANNELS - 2; i++) {
    this->dark_current_offset_[i + 2] = vals[i];
  }
}

void AS7341Component::set_channel_correction(const CalibrationCoeffs &vals) {
  // Copy first 4 values
  for (int i = 0; i < 4; i++) {
    this->channel_correction_[i] = vals[i];
  }

  // Set positions 4 and 5 to 1.0
  this->channel_correction_[4] = 1.0f;
  this->channel_correction_[5] = 1.0f;

  // Copy remaining values starting at position 6
  for (int i = 4; i < AS7341_NUM_CHANNELS - 2; i++) {
    this->channel_correction_[i + 2] = vals[i];
  }
}

void AS7341Component::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");
    this->state_ = State::START_MEASUREMENT;
  } else {
    ESP_LOGV(TAG, "Can't initiate new data collection - component not ready");
  }
}

float calc_tint_us(uint8_t atime, uint16_t astep) { return (1.0f + atime) * (1.0f + astep) * 2.78f; }

void AS7341Component::loop() {
  if (this->is_ready()) {
    switch (this->state_) {
      case State::IDLE:
        // doing nothing, having best time
        break;

      case State::START_MEASUREMENT:
        ESP_LOGE(TAG, "START_MEASUREMENT");
        // start measurement
        this->readings_.millis_start = millis();
        this->enable_spectral_measurement(false);

        this->setup_atime(this->atime_);
        this->setup_astep(this->astep_);
        this->setup_gain(this->gain_);
        this->readings_.low_success = false;
        this->readings_.high_success = false;
        this->state_ = State::ENABLE_SMUX_LOW;

        // uint32_t wait_ms = calc_tint_us(this->atime_, this->astep_) / 1000;
        // this->set_timeout(2 * wait_ms, [this]() { this->state_ = State::COLLECTING_DATA; });
        break;

      case State::ENABLE_SMUX_LOW: {
        ESP_LOGE(TAG, "ENABLE_SMUX_LOW");
        this->readings_.first_run = true;  // to discard first read
        this->set_smux_command(AS7341_SMUX_CMD_WRITE);
        this->configure_smux_low_channels();
        this->enable_smux();
        this->enable_spectral_measurement(true);
        this->state_ = State::WAIT_SMUX_LOW;
      } break;

      case State::WAIT_SMUX_LOW: {
        ESP_LOGE(TAG, "WAIT_SMUX_1");
        if (this->is_data_ready()) {
          if (this->readings_.first_run) {
            this->readings_.first_run = false;
            this->read_and_discard_channels_();  // discard first read, data is unstable
          } else {
            this->state_ = State::READ_SMUX_LOW;
          }
        } else if (millis() - this->readings_.millis_start > DATA_COLLECTION_TIMEOUT_MS) {
          ESP_LOGW(TAG, "Data collection timeout");
          this->state_ = State::IDLE;
        }
      } break;

      case State::READ_SMUX_LOW: {
        ESP_LOGE(TAG, "READ_SMUX_LOW");
        this->read_low_channels_();
        this->enable_spectral_measurement(false);
        this->state_ = State::ENABLE_SMUX_HIGH;
      } break;

      case State::ENABLE_SMUX_HIGH: {
        ESP_LOGE(TAG, "ENABLE_SMUX_HIGH");
        this->readings_.first_run = true;  // to discard first read
        this->set_smux_command(AS7341_SMUX_CMD_WRITE);
        this->configure_smux_high_channels();
        this->enable_smux();
        this->enable_spectral_measurement(true);
        this->state_ = State::WAIT_SMUX_HIGH;
      } break;

      case State::WAIT_SMUX_HIGH: {
        ESP_LOGE(TAG, "WAIT_SMUX_HIGH");
        if (this->is_data_ready()) {
          if (this->readings_.first_run) {
            this->readings_.first_run = false;
            this->read_and_discard_channels_();  // discard first read, data is unstable
          } else {
            this->state_ = State::READ_SMUX_HIGH;
          }
        } else if (millis() - this->readings_.millis_start > DATA_COLLECTION_TIMEOUT_MS) {
          ESP_LOGW(TAG, "Data collection timeout");
          this->state_ = State::IDLE;
        }
      } break;

      case State::READ_SMUX_HIGH: {
        ESP_LOGE(TAG, "READ_SMUX_2");
        this->read_high_channels_();
        this->enable_spectral_measurement(false);
        this->state_ = State::DATA_COLLECTED;
      } break;

      case State::DATA_COLLECTED: {
        ESP_LOGE(TAG, "DATA_COLLECTED");

        // think
        if (this->readings_.low_gain != this->readings_.high_gain) {
          ESP_LOGW(TAG, "Gain mismatch: low gain %u, high gain %u", this->readings_.low_gain,
                   this->readings_.high_gain);
        }
        this->readings_.gain = this->readings_.high_gain;

        this->readings_.gain_x = this->get_gain_multiplier(this->readings_.gain);
        this->readings_.atime = this->get_atime();
        this->readings_.astep = this->get_astep();
        this->readings_.t_int_us = (1.0f + this->readings_.atime) * (1.0f + this->readings_.astep) * 2.78f;

        this->calculate_basic_counts_();
        uint16_t max_adc = this->get_maximum_spectral_adc_(this->readings_.atime, this->readings_.astep);
        uint16_t highest_adc = this->get_highest_value(this->readings_.raw_counts);
        this->calculated_values_.saturation_level = (max_adc == 0) ? 0 : 100.0f * highest_adc / (float) max_adc;

        ESP_LOGD(TAG, "Parameters:");
        ESP_LOGD(TAG, "  Gain  =  %.1f", this->readings_.gain_x);
        ESP_LOGD(TAG, "  ATIME = %u ", this->readings_.atime);
        ESP_LOGD(TAG, "  ASTEP = %u ", this->readings_.astep);
        ESP_LOGD(TAG, "  TINT  = %.4f ms", this->readings_.t_int_us / 1000.0f);
        ESP_LOGD(TAG, "Readings:");
        ESP_LOGD(TAG, "  Max ADC: %u", max_adc);
        ESP_LOGD(TAG, "  Highest ADC: %.2f%%", this->calculated_values_.saturation_level);
        log_cn_s(TAG, "  Channel", CHANNEL_NAME);
        log_cn_d(TAG, "  Nm", CHANNEL_WAVE_NM);
        log_cn_d(TAG, "  Counts", this->readings_.raw_counts);
        log_cn_f(TAG, "    - Corr Dark", this->dark_current_offset_);
        log_cn_f(TAG, "    * Corr Coeff", this->channel_correction_);
        ESP_LOGD(TAG, "    * Glass attn. %.2f", this->glass_attenuation_factor_);
        log_cn_f(TAG, "  Basic", this->calculated_values_.basic_counts);

        this->state_ = State::CALCULATE_CIE;
      } break;

      case State::CALCULATE_CIE: {
        ESP_LOGE(TAG, "CALCULATE_CIE");
        this->calculate_color_(this->calculated_values_.cct, this->calculated_values_.duv,
                               this->calculated_values_.lux_from_xyz);
        this->state_ = State::CALCULATE_SPECTRAL;
      } break;

      case State::CALCULATE_SPECTRAL: {
        ESP_LOGE(TAG, "CALCULATE_SPECTRAL");
        this->calculate_spectral_(this->calculated_values_.lux_from_irradiance, this->calculated_values_.irradiance_par,
                                  this->calculated_values_.ppfd, this->calculated_values_.irradiance,
                                  this->calculated_values_.irradiance_photopic);
        this->state_ = State::VALIDATE_VALUES;
      } break;

      case State::VALIDATE_VALUES: {
        ESP_LOGE(TAG, "VALIDATE_VALUES");
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
        this->calculated_values_.duv = std::max(this->calculated_values_.duv, 0.0f);

        ESP_LOGD(TAG, "Calculated values (glass attenuation = %.2f):", this->glass_attenuation_factor_);
        ESP_LOGD(TAG, "  CIE");
        ESP_LOGD(TAG, "    Color temp(XYZ)     : %f K", this->calculated_values_.cct);
        ESP_LOGD(TAG, "    Duv(XYZ)            : %f", this->calculated_values_.duv);
        ESP_LOGD(TAG, "    Illuminance(XYZ)    : %f lx", this->calculated_values_.lux_from_xyz);
        ESP_LOGD(TAG, "  Spectral");
        ESP_LOGD(TAG, "    Irradiance          : %f W/m²", this->calculated_values_.irradiance);
        ESP_LOGD(TAG, "    Irradiance(photopic): %f W/m²", this->calculated_values_.irradiance_photopic);
        ESP_LOGD(TAG, "    PAR                 : %f W/m²", this->calculated_values_.irradiance_par);
        ESP_LOGD(TAG, "    PPFD                : %f µmol/s⋅m²", this->calculated_values_.ppfd);
        ESP_LOGD(TAG, "    Illuminance         : %f lx", this->calculated_values_.lux_from_irradiance);

        float &cct = this->calculated_values_.cct;
        constexpr float CCT_LOW_LIMIT = 2865.0f;
        constexpr float CCT_HIGH_LIMIT = 10000.f;  // 6500.0f;
        if (cct < CCT_LOW_LIMIT) {
          ESP_LOGW(TAG, "CCT is too low: %f K", cct);
          cct = CCT_LOW_LIMIT;
        } else if (cct > CCT_HIGH_LIMIT) {
          ESP_LOGW(TAG, "CCT is too high: %f K", cct);
          cct = CCT_HIGH_LIMIT;
        }

        constexpr float LUX_MAX = 125000.0f;
        this->calculated_values_.lux = std::min(this->calculated_values_.lux_from_xyz, LUX_MAX);
        ESP_LOGD(TAG, "  Clamping values");
        ESP_LOGD(TAG, "    Color temp(XYZ)     : %f K", this->calculated_values_.cct);
        ESP_LOGD(TAG, "    Illuminance (XYZ)   : %f lx", this->calculated_values_.lux);
        this->state_ = State::READY_TO_PUBLISH_PART_1;
      } break;

      case State::READY_TO_PUBLISH_PART_1:
        ESP_LOGE(TAG, "READY_TO_PUBLISH_PART_1");
        this->publish_channel_readings_();
        this->state_ = State::READY_TO_PUBLISH_PART_2;
        break;

      case State::READY_TO_PUBLISH_PART_2:
        ESP_LOGE(TAG, "READY_TO_PUBLISH_PART_2");
        this->publish_basic_counts_();
        this->state_ = State::READY_TO_PUBLISH_PART_3;
        break;

      case State::READY_TO_PUBLISH_PART_3:
        ESP_LOGE(TAG, "READY_TO_PUBLISH_PART_3");
        this->publish_derived_readings_();
        this->state_ = State::IDLE;
        break;
    }
  }
}

void AS7341Component::publish_channel_readings_() {
  for (int i = 0; i < AS7341_NUM_CHANNELS; i++) {
    this->publish_sensor(this->band_counts_sensors_[i], this->readings_.raw_counts[i]);
  }
}

void AS7341Component::publish_basic_counts_() {
  for (int i = 0; i < AS7341_NUM_CHANNELS; i++) {
    this->publish_sensor(this->band_basic_counts_sensors_[i], this->calculated_values_.basic_counts[i]);
  }
}

void AS7341Component::publish_derived_readings_() {
  this->publish_sensor(this->illuminance_sensor_, this->calculated_values_.lux);
  this->publish_sensor(this->irradiance_sensor_, this->calculated_values_.irradiance);
  this->publish_sensor(this->irradiance_photopic_sensor_, this->calculated_values_.irradiance_photopic);
  this->publish_sensor(this->irradiance_par_sensor_, this->calculated_values_.irradiance_par);
  this->publish_sensor(this->ppfd_sensor_, this->calculated_values_.ppfd);
  this->publish_sensor(this->color_temperature_sensor_, this->calculated_values_.cct);
  this->publish_sensor(this->saturation_level_sensor_, this->calculated_values_.saturation_level);
  // this->publish_sensor(this->saturated_, this->readings_.saturated);
}

uint16_t AS7341Component::get_maximum_spectral_adc_() {
  return this->get_maximum_spectral_adc_(this->atime_, this->astep_);
}

uint16_t AS7341Component::get_maximum_spectral_adc_(uint16_t atime, uint16_t astep) {
  static constexpr uint16_t MAX_ADC_COUNT = 65535;
  uint32_t value = (atime + 1) * (astep + 1);
  if (value > MAX_ADC_COUNT) {
    value = MAX_ADC_COUNT;
  }
  return value;
}

template<typename T, size_t N> T AS7341Component::get_highest_value(std::array<T, N> &data) {
  T max = 0;
  for (const auto &v : data) {
    if (v > max) {
      max = v;
    }
  }
  return max;
}

void AS7341Component::calculate_basic_counts_() {
  this->calculated_values_.basic_counts.fill(0.0f);
  const float inv_exposure = 1.0f / (this->readings_.gain_x * this->readings_.t_int_us);

  for (size_t i = 0; i < AS7341_NUM_CHANNELS; i++) {
    float basic_count = this->readings_.raw_counts[i] * inv_exposure;

    basic_count -= this->dark_current_offset_[i];
    basic_count = std::max(basic_count, 0.0f);
    basic_count *= AS7341_GAIN_CORRECTION[(uint8_t) this->readings_.gain];
    basic_count *= this->channel_correction_[i];
    basic_count *= this->glass_attenuation_factor_;

    // // μW/cm² -> mW/m²
    // constexpr float INV_CONVERT_MW_PER_CM2_TO_MW_PER_M2 = 0.1f;
    // basic_count *= INV_CONVERT_MW_PER_CM2_TO_MW_PER_M2;

    this->calculated_values_.basic_counts[i] = basic_count;
  }
}

void AS7341Component::calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance,
                                          float &irradiance_photopic) {
  lux = ppfd = par = irradiance = irradiance_photopic = 0.0f;

  for (uint8_t i = 0; i < AS7341_NUM_CHANNELS; i++) {
    float bc = this->calculated_values_.basic_counts[i];
    irradiance += AS7341_IRRAD_MW_PER_COUNT[i] * bc;

    irradiance_photopic += AS7341_IRRAD_PHOTOPIC_MW_PER_COUNT[i] * bc;
    par += AS7341_IRRAD_PAR_E_MW_PER_COUNT[i] * bc;
    ppfd += AS7341_PPFD_UMOL_PER_COUNT[i] * bc;
  }

  lux = irradiance_photopic * 683.002f;

  ppfd *= 1e3f;  // convert to µmol/s·m²
}

void AS7341Component::calculate_color_(float &cct, float &duv, float &lux) {
  ESP_LOGD(TAG, "Tristimulus:");
  float XYZ[3] = {0, 0, 0};
  for (uint8_t i = 0; i < AS7341_NUM_CHANNELS; i++) {
    for (uint8_t j = 0; j < 3; j++) {
      XYZ[j] += AS7341_XYZ_PER_COUNT[j][i] * this->calculated_values_.basic_counts[i];
    }
  }

  constexpr float epsilon = 0.0001;
  float xyz_sum = XYZ[0] + XYZ[1] + XYZ[2];
  float x{0}, y{0}, z{0};
  if (fabs(xyz_sum) < epsilon) {
    cct = 0;
    duv = NAN;
    lux = 0;
  } else {
    duv = NAN;
    x = XYZ[0] / xyz_sum;
    y = XYZ[1] / xyz_sum;
    z = XYZ[2] / xyz_sum;

    float n = (x - 0.3320) / (0.1858 - y);

    cct = 437 * pow(n, 3) + 3601 * pow(n, 2) + 6861 * n + 5517;

    float &X = XYZ[0];
    float &Y = XYZ[1];
    float &Z = XYZ[2];
    float uprime;
    float vprime;

    uprime = 4 * X / (X + 15 * Y + 3 * Z);
    vprime = 9 * Y / (X + 15 * Y + 3 * Z);

    {
      double u = (4 * x) / (-2 * x + 12 * y + 3);
      double v = (6 * y) / (-2 * x + 12 * y + 3);

      const double k6 = -0.00616793;
      const double k5 = 0.0893944;
      const double k4 = -0.5179722;
      const double k3 = 1.5317403;
      const double k2 = -2.4243787;
      const double k1 = 1.925865;
      const double k0 = -0.471106;

      double Lfp = sqrt(pow((u - 0.292), 2) + pow((v - 0.24), 2));
      double a = acos((u - 0.292) / Lfp);
      double Lbb = k6 * pow(a, 6) + k5 * pow(a, 5) + k4 * pow(a, 4) + k3 * pow(a, 3) + k2 * pow(a, 2) + k1 * a + k0;

      duv = static_cast<float>(Lfp - Lbb);
    }

    // Calc lx from Y CIE1931
    lux = XYZ[1] * this->corr_lx_y_cie1931_;

    ESP_LOGD(TAG, "  XYZ: %.4f, %.4f, %.4f", X, Y, Z);
    ESP_LOGD(TAG, "  xyz: %.4f, %.4f, %.4f", x, y, z);
    ESP_LOGD(TAG, "  CCT: %.2f, u': %.4f, v': %.4f, duv: %.2f", cct, uprime, vprime, duv);
  }
}

float AS7341Component::get_gain_multiplier(AS7341Gain gain) {
  float gainx = ((uint16_t) 1 << (uint8_t) gain);
  // The AS7341 sensor's gain values are represented as powers of 2, but the actual gain multiplier
  // is half of this value. This division by 2 adjusts the calculated gain multiplier accordingly.
  gainx /= 2.0f;
  return gainx;
}

AS7341Gain AS7341Component::get_gain() {
  uint8_t data;
  this->read_byte(AS7341_CFG1, &data);
  return (AS7341Gain) data;
}

uint8_t AS7341Component::get_atime() {
  uint8_t data;
  this->read_byte(AS7341_ATIME, &data);
  return data;
}

uint16_t AS7341Component::get_astep() {
  uint16_t data;
  this->read_byte_16(AS7341_ASTEP, &data);
  return this->swap_bytes(data);
}

bool AS7341Component::setup_gain(AS7341Gain gain) { return this->write_byte(AS7341_CFG1, gain); }

bool AS7341Component::setup_atime(uint8_t atime) { return this->write_byte(AS7341_ATIME, atime); }

bool AS7341Component::setup_astep(uint16_t astep) { return this->write_byte_16(AS7341_ASTEP, swap_bytes(astep)); }

bool AS7341Component::read_low_channels_() {
  auto data = this->readings_.raw_counts.data();
  AS7341RegAStatus low_astatus{};
  this->read_byte(AS7341_ASTATUS, &low_astatus.raw);
  this->readings_.low_success = this->read_bytes_16(AS7341_CH0_DATA_L, data, 6);
  this->readings_.low_gain = low_astatus.again_status;
  this->readings_.low_saturated = low_astatus.asat_status;
  float gainx = this->get_gain_multiplier(low_astatus.again_status);
  ESP_LOGD(TAG, "read low channels. gainx %.1f, saturated %d", gainx, low_astatus.asat_status);
  ESP_LOGD(TAG, "low_success %d", this->readings_.low_success);
  return this->readings_.low_success;
}

bool AS7341Component::read_high_channels_() {
  auto data = this->readings_.raw_counts.data();
  AS7341RegAStatus high_astatus{};
  this->read_byte(AS7341_ASTATUS, &high_astatus.raw);
  this->readings_.high_success = this->read_bytes_16(AS7341_CH0_DATA_L, &data[6], 6);
  this->readings_.high_gain = high_astatus.again_status;
  this->readings_.high_saturated = high_astatus.asat_status;
  float gainx = this->get_gain_multiplier(high_astatus.again_status);
  ESP_LOGD(TAG, "read high channels. gainx %.1f, saturated %d", gainx, high_astatus.asat_status);
  ESP_LOGD(TAG, "high_success %d", this->readings_.high_success);
  return this->readings_.high_success;
}

bool AS7341Component::read_and_discard_channels_() {
  uint16_t data[AS7341_NUM_CHANNELS];
  return this->read_bytes_16(AS7341_CH0_DATA_L, data, 6);
}

bool AS7341Component::set_smux_command(AS7341SmuxCommand command) {
  uint8_t data = command << 3;  // Write to bits 4:3 of the register
  return this->write_byte(AS7341_CFG6, data);
}

void AS7341Component::configure_smux_low_channels() {
  // SMUX Config for F1,F2,F3,F4,NIR,Clear
  this->write_byte(0x00, 0x30);  // F3 left set to ADC2
  this->write_byte(0x01, 0x01);  // F1 left set to ADC0
  this->write_byte(0x02, 0x00);  // Reserved or disabled
  this->write_byte(0x03, 0x00);  // F8 left disabled
  this->write_byte(0x04, 0x00);  // F6 left disabled
  this->write_byte(0x05, 0x42);  // F4 left connected to ADC3/f2 left connected to ADC1
  this->write_byte(0x06, 0x00);  // F5 left disbled
  this->write_byte(0x07, 0x00);  // F7 left disbled
  this->write_byte(0x08, 0x50);  // CLEAR connected to ADC4
  this->write_byte(0x09, 0x00);  // F5 right disabled
  this->write_byte(0x0A, 0x00);  // F7 right disabled
  this->write_byte(0x0B, 0x00);  // Reserved or disabled
  this->write_byte(0x0C, 0x20);  // F2 right connected to ADC1
  this->write_byte(0x0D, 0x04);  // F4 right connected to ADC3
  this->write_byte(0x0E, 0x00);  // F6/F8 right disabled
  this->write_byte(0x0F, 0x30);  // F3 right connected to AD2
  this->write_byte(0x10, 0x01);  // F1 right connected to AD0
  this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
  this->write_byte(0x12, 0x00);  // Reserved or disabled
  this->write_byte(0x13, 0x06);  // NIR connected to ADC5
}

void AS7341Component::configure_smux_high_channels() {
  // SMUX Config for F5,F6,F7,F8,NIR,Clear
  this->write_byte(0x00, 0x00);  // F3 left disable
  this->write_byte(0x01, 0x00);  // F1 left disable
  this->write_byte(0x02, 0x00);  // reserved/disable
  this->write_byte(0x03, 0x40);  // F8 left connected to ADC3
  this->write_byte(0x04, 0x02);  // F6 left connected to ADC1
  this->write_byte(0x05, 0x00);  // F4/ F2 disabled
  this->write_byte(0x06, 0x10);  // F5 left connected to ADC0
  this->write_byte(0x07, 0x03);  // F7 left connected to ADC2
  this->write_byte(0x08, 0x50);  // CLEAR Connected to ADC4
  this->write_byte(0x09, 0x10);  // F5 right connected to ADC0
  this->write_byte(0x0A, 0x03);  // F7 right connected to ADC2
  this->write_byte(0x0B, 0x00);  // Reserved or disabled
  this->write_byte(0x0C, 0x00);  // F2 right disabled
  this->write_byte(0x0D, 0x00);  // F4 right disabled
  this->write_byte(0x0E, 0x24);  // F8 right connected to ADC2/ F6 right connected to ADC1
  this->write_byte(0x0F, 0x00);  // F3 right disabled
  this->write_byte(0x10, 0x00);  // F1 right disabled
  this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
  this->write_byte(0x12, 0x00);  // Reserved or disabled
  this->write_byte(0x13, 0x06);  // NIR connected to ADC5
}

bool AS7341Component::enable_smux() {
  this->set_register_bit(AS7341_ENABLE, 4);

  uint16_t timeout = 1000;
  for (uint16_t time = 0; time < timeout; time++) {
    // The SMUXEN bit is cleared once the SMUX operation is finished
    bool smuxen = this->read_register_bit(AS7341_ENABLE, 4);
    if (!smuxen) {
      return true;
    }

    delay(1);
  }

  return false;
}

bool AS7341Component::is_data_ready() {
  AS7341RegStatus2 status2{0};
  this->read_byte(AS7341_STATUS2, &status2.raw);
  if (status2.avalid) {
    ESP_LOGD(TAG, "Status2 0x%02x, avalid %d, asat_digital %d, asat_analog %d", status2.raw, status2.avalid,
             status2.asat_digital, status2.asat_analog);
  }

  return status2.avalid;
}

bool AS7341Component::enable_power(bool enable) { return this->write_register_bit(AS7341_ENABLE, enable, 0); }

bool AS7341Component::enable_spectral_measurement(bool enable) {
  return this->write_register_bit(AS7341_ENABLE, enable, 1);
}

bool AS7341Component::read_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS7341Component::write_register_bit(uint8_t address, bool value, uint8_t bit_position) {
  if (value) {
    return this->set_register_bit(address, bit_position);
  }

  return this->clear_register_bit(address, bit_position);
}

bool AS7341Component::set_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->write_byte(address, data);
}

bool AS7341Component::clear_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->write_byte(address, data);
}

uint16_t AS7341Component::swap_bytes(uint16_t data) { return (data >> 8) | (data << 8); }

}  // namespace esphome::as7341
