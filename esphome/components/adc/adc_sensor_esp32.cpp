#ifdef USE_ESP32

#include "adc_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc {

static const char *const TAG = "adc.esp32";

adc_oneshot_unit_handle_t ADCSensor::shared_adc_handles[2] = {nullptr, nullptr};

const LogString *attenuation_to_str(adc_atten_t attenuation) {
  switch (attenuation) {
    case ADC_ATTEN_DB_0:
      return LOG_STR("0 dB");
    case ADC_ATTEN_DB_2_5:
      return LOG_STR("2.5 dB");
    case ADC_ATTEN_DB_6:
      return LOG_STR("6 dB");
    case ADC_ATTEN_DB_12_COMPAT:
      return LOG_STR("12 dB");
    default:
      return LOG_STR("Unknown Attenuation");
  }
}

const LogString *adc_unit_to_str(adc_unit_t unit) {
  switch (unit) {
    case ADC_UNIT_1:
      return LOG_STR("ADC1");
    case ADC_UNIT_2:
      return LOG_STR("ADC2");
    default:
      return LOG_STR("Unknown ADC Unit");
  }
}

void ADCSensor::setup() {
  // Check if another sensor already initialized this ADC unit
  if (ADCSensor::shared_adc_handles[this->adc_unit_] == nullptr) {
    adc_oneshot_unit_init_cfg_t init_config = {};  // Zero initialize
    init_config.unit_id = this->adc_unit_;
    init_config.ulp_mode = ADC_ULP_MODE_DISABLE;
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2
    init_config.clk_src = ADC_DIGI_CLK_SRC_DEFAULT;
#endif  // USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 ||
        // USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2
    esp_err_t err = adc_oneshot_new_unit(&init_config, &ADCSensor::shared_adc_handles[this->adc_unit_]);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error initializing %s: %d", LOG_STR_ARG(adc_unit_to_str(this->adc_unit_)), err);
      this->mark_failed();
      return;
    }
  }
  this->adc_handle_ = ADCSensor::shared_adc_handles[this->adc_unit_];

  this->setup_flags_.handle_init_complete = true;

  adc_oneshot_chan_cfg_t config = {
      .atten = this->attenuation_,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  esp_err_t err = adc_oneshot_config_channel(this->adc_handle_, this->channel_, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error configuring channel: %d", err);
    this->mark_failed();
    return;
  }
  this->setup_flags_.config_complete = true;

  // Initialize ADC calibration
  if (this->calibration_handle_ == nullptr) {
    adc_cali_handle_t handle = nullptr;

#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
    // RISC-V variants and S3 use curve fitting calibration
    adc_cali_curve_fitting_config_t cali_config = {};  // Zero initialize first
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    cali_config.chan = this->channel_;
#endif  // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    cali_config.unit_id = this->adc_unit_;
    cali_config.atten = this->attenuation_;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;

    err = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (err == ESP_OK) {
      this->calibration_handle_ = handle;
      this->setup_flags_.calibration_complete = true;
      ESP_LOGV(TAG, "Using curve fitting calibration");
    } else {
      ESP_LOGW(TAG, "Curve fitting calibration failed with error %d, will use uncalibrated readings", err);
      this->setup_flags_.calibration_complete = false;
    }
#else  // Other ESP32 variants use line fitting calibration
    adc_cali_line_fitting_config_t cali_config = {
      .unit_id = this->adc_unit_,
      .atten = this->attenuation_,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
#if !defined(USE_ESP32_VARIANT_ESP32S2)
      .default_vref = 1100,  // Default reference voltage in mV
#endif  // !defined(USE_ESP32_VARIANT_ESP32S2)
    };
    err = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (err == ESP_OK) {
      this->calibration_handle_ = handle;
      this->setup_flags_.calibration_complete = true;
      ESP_LOGV(TAG, "Using line fitting calibration");
    } else {
      ESP_LOGW(TAG, "Line fitting calibration failed with error %d, will use uncalibrated readings", err);
      this->setup_flags_.calibration_complete = false;
    }
#endif  // USE_ESP32_VARIANT_ESP32C3 || ESP32C5 || ESP32C6 || ESP32C61 || ESP32H2 || ESP32P4 || ESP32S3
  }

  this->setup_flags_.init_complete = true;
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG,
                "  Channel:       %d\n"
                "  Unit:          %s\n"
                "  Attenuation:   %s\n"
                "  Samples:       %i\n"
                "  Sampling mode: %s",
                this->channel_, LOG_STR_ARG(adc_unit_to_str(this->adc_unit_)),
                this->autorange_ ? "Auto" : LOG_STR_ARG(attenuation_to_str(this->attenuation_)), this->sample_count_,
                LOG_STR_ARG(sampling_mode_to_str(this->sampling_mode_)));

  ESP_LOGCONFIG(
      TAG,
      "  Setup Status:\n"
      "    Handle Init:  %s\n"
      "    Config:       %s\n"
      "    Calibration:  %s\n"
      "    Overall Init: %s",
      this->setup_flags_.handle_init_complete ? "OK" : "FAILED", this->setup_flags_.config_complete ? "OK" : "FAILED",
      this->setup_flags_.calibration_complete ? "OK" : "FAILED", this->setup_flags_.init_complete ? "OK" : "FAILED");

  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  if (this->autorange_) {
    return this->sample_autorange_();
  } else {
    return this->sample_fixed_attenuation_();
  }
}

float ADCSensor::sample_fixed_attenuation_() {
  auto aggr = Aggregator<uint32_t>(this->sampling_mode_);

  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
    int raw;
    esp_err_t err = adc_oneshot_read(this->adc_handle_, this->channel_, &raw);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "ADC read failed with error %d", err);
      continue;
    }

    if (raw == -1) {
      ESP_LOGW(TAG, "Invalid ADC reading");
      continue;
    }

    aggr.add_sample(raw);
  }

  uint32_t final_value = aggr.aggregate();

  if (this->output_raw_) {
    return final_value;
  }

  if (this->calibration_handle_ != nullptr) {
    int voltage_mv;
    esp_err_t err = adc_cali_raw_to_voltage(this->calibration_handle_, final_value, &voltage_mv);
    if (err == ESP_OK) {
      return voltage_mv / 1000.0f;
    } else {
      ESP_LOGW(TAG, "ADC calibration conversion failed with error %d, disabling calibration", err);
      if (this->calibration_handle_ != nullptr) {
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
        adc_cali_delete_scheme_curve_fitting(this->calibration_handle_);
#else   // Other ESP32 variants use line fitting calibration
        adc_cali_delete_scheme_line_fitting(this->calibration_handle_);
#endif  // USE_ESP32_VARIANT_ESP32C3 || ESP32C5 || ESP32C6 || ESP32C61 || ESP32H2 || ESP32P4 || ESP32S3
        this->calibration_handle_ = nullptr;
      }
    }
  }

  return final_value * 3.3f / 4095.0f;
}

float ADCSensor::sample_autorange_() {
  // Auto-range mode
  auto read_atten = [this](adc_atten_t atten) -> std::pair<int, float> {
    // First reconfigure the attenuation for this reading
    adc_oneshot_chan_cfg_t config = {
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t err = adc_oneshot_config_channel(this->adc_handle_, this->channel_, &config);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error configuring ADC channel for autorange: %d", err);
      return {-1, 0.0f};
    }

    // Need to recalibrate for the new attenuation
    if (this->calibration_handle_ != nullptr) {
      // Delete old calibration handle
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
      adc_cali_delete_scheme_curve_fitting(this->calibration_handle_);
#else
      adc_cali_delete_scheme_line_fitting(this->calibration_handle_);
#endif
      this->calibration_handle_ = nullptr;
    }

    // Create new calibration handle for this attenuation
    adc_cali_handle_t handle = nullptr;

#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
    adc_cali_curve_fitting_config_t cali_config = {};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    cali_config.chan = this->channel_;
#endif
    cali_config.unit_id = this->adc_unit_;
    cali_config.atten = atten;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;

    err = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    ESP_LOGVV(TAG, "Autorange atten=%d: Calibration handle creation %s (err=%d)", atten,
              (err == ESP_OK) ? "SUCCESS" : "FAILED", err);
#else
    adc_cali_line_fitting_config_t cali_config = {
      .unit_id = this->adc_unit_,
      .atten = atten,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
#if !defined(USE_ESP32_VARIANT_ESP32S2)
      .default_vref = 1100,
#endif
    };
    err = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    ESP_LOGVV(TAG, "Autorange atten=%d: Calibration handle creation %s (err=%d)", atten,
              (err == ESP_OK) ? "SUCCESS" : "FAILED", err);
#endif

    int raw;
    err = adc_oneshot_read(this->adc_handle_, this->channel_, &raw);
    ESP_LOGVV(TAG, "Autorange atten=%d: Raw ADC read %s, value=%d (err=%d)", atten,
              (err == ESP_OK) ? "SUCCESS" : "FAILED", raw, err);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "ADC read failed in autorange with error %d", err);
      if (handle != nullptr) {
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
        adc_cali_delete_scheme_curve_fitting(handle);
#else
        adc_cali_delete_scheme_line_fitting(handle);
#endif
      }
      return {-1, 0.0f};
    }

    float voltage = 0.0f;
    if (handle != nullptr) {
      int voltage_mv;
      err = adc_cali_raw_to_voltage(handle, raw, &voltage_mv);
      if (err == ESP_OK) {
        voltage = voltage_mv / 1000.0f;
        ESP_LOGVV(TAG, "Autorange atten=%d: CALIBRATED - raw=%d -> %dmV -> %.6fV", atten, raw, voltage_mv, voltage);
      } else {
        voltage = raw * 3.3f / 4095.0f;
        ESP_LOGVV(TAG, "Autorange atten=%d: UNCALIBRATED FALLBACK - raw=%d -> %.6fV (3.3V ref)", atten, raw, voltage);
      }
      // Clean up calibration handle
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
      adc_cali_delete_scheme_curve_fitting(handle);
#else
      adc_cali_delete_scheme_line_fitting(handle);
#endif
    } else {
      voltage = raw * 3.3f / 4095.0f;
      ESP_LOGVV(TAG, "Autorange atten=%d: NO CALIBRATION - raw=%d -> %.6fV (3.3V ref)", atten, raw, voltage);
    }

    return {raw, voltage};
  };

  auto [raw12, mv12] = read_atten(ADC_ATTEN_DB_12);
  if (raw12 == -1) {
    ESP_LOGE(TAG, "Failed to read ADC in autorange mode");
    return NAN;
  }

  int raw6 = 4095, raw2 = 4095, raw0 = 4095;
  float mv6 = 0, mv2 = 0, mv0 = 0;

  if (raw12 < 4095) {
    auto [raw6_val, mv6_val] = read_atten(ADC_ATTEN_DB_6);
    raw6 = raw6_val;
    mv6 = mv6_val;

    if (raw6 < 4095 && raw6 != -1) {
      auto [raw2_val, mv2_val] = read_atten(ADC_ATTEN_DB_2_5);
      raw2 = raw2_val;
      mv2 = mv2_val;

      if (raw2 < 4095 && raw2 != -1) {
        auto [raw0_val, mv0_val] = read_atten(ADC_ATTEN_DB_0);
        raw0 = raw0_val;
        mv0 = mv0_val;
      }
    }
  }

  if (raw0 == -1 || raw2 == -1 || raw6 == -1 || raw12 == -1) {
    return NAN;
  }

  const int adc_half = 2048;
  const uint32_t c12 = std::min(raw12, adc_half);

  const int32_t c6_signed = adc_half - std::abs(raw6 - adc_half);
  const uint32_t c6 = (c6_signed > 0) ? c6_signed : 0;  // Clamp to prevent underflow

  const int32_t c2_signed = adc_half - std::abs(raw2 - adc_half);
  const uint32_t c2 = (c2_signed > 0) ? c2_signed : 0;  // Clamp to prevent underflow

  const uint32_t c0 = std::min(4095 - raw0, adc_half);
  const uint32_t csum = c12 + c6 + c2 + c0;

  ESP_LOGVV(TAG, "Autorange summary:");
  ESP_LOGVV(TAG, "  Raw readings: 12db=%d, 6db=%d, 2.5db=%d, 0db=%d", raw12, raw6, raw2, raw0);
  ESP_LOGVV(TAG, "  Voltages: 12db=%.6f, 6db=%.6f, 2.5db=%.6f, 0db=%.6f", mv12, mv6, mv2, mv0);
  ESP_LOGVV(TAG, "  Coefficients: c12=%u, c6=%u, c2=%u, c0=%u, sum=%u", c12, c6, c2, c0, csum);

  if (csum == 0) {
    ESP_LOGE(TAG, "Invalid weight sum in autorange calculation");
    return NAN;
  }

  const float final_result = (mv12 * c12 + mv6 * c6 + mv2 * c2 + mv0 * c0) / csum;
  ESP_LOGV(TAG, "Autorange final: (%.6f*%u + %.6f*%u + %.6f*%u + %.6f*%u)/%u = %.6fV", mv12, c12, mv6, c6, mv2, c2, mv0,
           c0, csum, final_result);

  return final_result;
}

}  // namespace adc
}  // namespace esphome

#endif  // USE_ESP32
