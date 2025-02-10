#ifdef USE_ESP32

#include "adc_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc {

static const char *const TAG = "adc.esp32";

static const adc_bits_width_t ADC_WIDTH_MAX_SOC_BITS = static_cast<adc_bits_width_t>(ADC_WIDTH_MAX - 1);

#ifndef SOC_ADC_RTC_MAX_BITWIDTH
#if USE_ESP32_VARIANT_ESP32S2
static const int32_t SOC_ADC_RTC_MAX_BITWIDTH = 13;
#else
static const int32_t SOC_ADC_RTC_MAX_BITWIDTH = 12;
#endif  // USE_ESP32_VARIANT_ESP32S2
#endif  // SOC_ADC_RTC_MAX_BITWIDTH

static const int ADC_MAX = (1 << SOC_ADC_RTC_MAX_BITWIDTH) - 1;
static const int ADC_HALF = (1 << SOC_ADC_RTC_MAX_BITWIDTH) >> 1;

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());

  if (this->channel1_ != ADC1_CHANNEL_MAX) {
    adc1_config_width(ADC_WIDTH_MAX_SOC_BITS);
    if (!this->autorange_) {
      adc1_config_channel_atten(this->channel1_, this->attenuation_);
    }
  } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
    if (!this->autorange_) {
      adc2_config_channel_atten(this->channel2_, this->attenuation_);
    }
  }

  for (int32_t i = 0; i <= ADC_ATTEN_DB_12_COMPAT; i++) {
    auto adc_unit = this->channel1_ != ADC1_CHANNEL_MAX ? ADC_UNIT_1 : ADC_UNIT_2;
    auto cal_value = esp_adc_cal_characterize(adc_unit, (adc_atten_t) i, ADC_WIDTH_MAX_SOC_BITS,
                                              1100,  // default vref
                                              &this->cal_characteristics_[i]);
    switch (cal_value) {
      case ESP_ADC_CAL_VAL_EFUSE_VREF:
        ESP_LOGV(TAG, "Using eFuse Vref for calibration");
        break;
      case ESP_ADC_CAL_VAL_EFUSE_TP:
        ESP_LOGV(TAG, "Using two-point eFuse Vref for calibration");
        break;
      case ESP_ADC_CAL_VAL_DEFAULT_VREF:
      default:
        break;
    }
  }
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
  if (this->autorange_) {
    ESP_LOGCONFIG(TAG, "  Attenuation: auto");
  } else {
    switch (this->attenuation_) {
      case ADC_ATTEN_DB_0:
        ESP_LOGCONFIG(TAG, "  Attenuation: 0db");
        break;
      case ADC_ATTEN_DB_2_5:
        ESP_LOGCONFIG(TAG, "  Attenuation: 2.5db");
        break;
      case ADC_ATTEN_DB_6:
        ESP_LOGCONFIG(TAG, "  Attenuation: 6db");
        break;
      case ADC_ATTEN_DB_12_COMPAT:
        ESP_LOGCONFIG(TAG, "  Attenuation: 12db");
        break;
      default:  // This is to satisfy the unused ADC_ATTEN_MAX
        break;
    }
  }
  ESP_LOGCONFIG(TAG, "  Samples: %i", this->sample_count_);
  ESP_LOGCONFIG(TAG, "  Sampling mode: %s", LOG_STR_ARG(sampling_mode_to_str(this->sampling_mode_)));
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  if (!this->autorange_) {
    auto aggr = Aggregator(this->sampling_mode_);

    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      int raw = -1;
      if (this->channel1_ != ADC1_CHANNEL_MAX) {
        raw = adc1_get_raw(this->channel1_);
      } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
        adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw);
      }
      if (raw == -1) {
        return NAN;
      }

      aggr.add_sample(raw);
    }
    if (this->output_raw_) {
      return aggr.aggregate();
    }
    uint32_t mv =
        esp_adc_cal_raw_to_voltage(aggr.aggregate(), &this->cal_characteristics_[(int32_t) this->attenuation_]);
    return mv / 1000.0f;
  }

  int raw12 = ADC_MAX, raw6 = ADC_MAX, raw2 = ADC_MAX, raw0 = ADC_MAX;

  if (this->channel1_ != ADC1_CHANNEL_MAX) {
    adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_12_COMPAT);
    raw12 = adc1_get_raw(this->channel1_);
    if (raw12 < ADC_MAX) {
      adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_6);
      raw6 = adc1_get_raw(this->channel1_);
      if (raw6 < ADC_MAX) {
        adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_2_5);
        raw2 = adc1_get_raw(this->channel1_);
        if (raw2 < ADC_MAX) {
          adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_0);
          raw0 = adc1_get_raw(this->channel1_);
        }
      }
    }
  } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
    adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_12_COMPAT);
    adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw12);
    if (raw12 < ADC_MAX) {
      adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_6);
      adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw6);
      if (raw6 < ADC_MAX) {
        adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_2_5);
        adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw2);
        if (raw2 < ADC_MAX) {
          adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_0);
          adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw0);
        }
      }
    }
  }

  if (raw0 == -1 || raw2 == -1 || raw6 == -1 || raw12 == -1) {
    return NAN;
  }

  uint32_t mv12 = esp_adc_cal_raw_to_voltage(raw12, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_12_COMPAT]);
  uint32_t mv6 = esp_adc_cal_raw_to_voltage(raw6, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_6]);
  uint32_t mv2 = esp_adc_cal_raw_to_voltage(raw2, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_2_5]);
  uint32_t mv0 = esp_adc_cal_raw_to_voltage(raw0, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_0]);

  uint32_t c12 = std::min(raw12, ADC_HALF);
  uint32_t c6 = ADC_HALF - std::abs(raw6 - ADC_HALF);
  uint32_t c2 = ADC_HALF - std::abs(raw2 - ADC_HALF);
  uint32_t c0 = std::min(ADC_MAX - raw0, ADC_HALF);
  uint32_t csum = c12 + c6 + c2 + c0;

  uint32_t mv_scaled = (mv12 * c12) + (mv6 * c6) + (mv2 * c2) + (mv0 * c0);
  return mv_scaled / (float) (csum * 1000U);
}

}  // namespace adc
}  // namespace esphome

#endif  // USE_ESP32
