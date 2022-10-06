#include "adc_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP8266
#ifdef USE_ADC_SENSOR_VCC
#include <Esp.h>
ADC_MODE(ADC_VCC)
#else
#include <Arduino.h>
#endif
#endif

namespace esphome {
namespace adc {

static const char *const TAG = "adc";

// 13bit for S2, and 12bit for all other esp32 variants
#ifdef USE_ESP32
static const adc_bits_width_t ADC_WIDTH_MAX_SOC_BITS = static_cast<adc_bits_width_t>(ADC_WIDTH_MAX - 1);

#ifndef SOC_ADC_RTC_MAX_BITWIDTH
#if USE_ESP32_VARIANT_ESP32S2
static const int SOC_ADC_RTC_MAX_BITWIDTH = 13;
#else
static const int SOC_ADC_RTC_MAX_BITWIDTH = 12;
#endif
#endif

static const int ADC_MAX = (1 << SOC_ADC_RTC_MAX_BITWIDTH) - 1;    // 4095 (12 bit) or 8191 (13 bit)
static const int ADC_HALF = (1 << SOC_ADC_RTC_MAX_BITWIDTH) >> 1;  // 2048 (12 bit) or 4096 (13 bit)

void config_channel_atten(const adc1_channel_t &channel1, const adc2_channel_t &channel2,
                          const adc_atten_t &attenuation, ADCSensor::Channel channel_num) {
  if (channel_num == ADCSensor::Channel::channel1) {
    adc1_config_channel_atten(channel1, attenuation);
  } else {
    adc2_config_channel_atten(channel2, attenuation);
  }
}
#endif

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#ifndef USE_ADC_SENSOR_VCC
  pin_->setup();
#endif

#ifdef USE_ESP32
  adc1_config_width(ADC_WIDTH_MAX_SOC_BITS);
  if (!autorange_) {
    config_channel_atten(channel1_, channel2_, attenuation_, channel_num_);
  }

  // load characteristics for each attenuation
  for (int i = 0; i < (int) ADC_ATTEN_MAX; i++) {
    auto cal_value = esp_adc_cal_characterize(ADC_UNIT_1, (adc_atten_t) i, ADC_WIDTH_MAX_SOC_BITS,
                                              1100,  // default vref
                                              &cal_characteristics_[i]);
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

#endif  // USE_ESP32
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
#ifdef USE_ESP8266
#ifdef USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else
  LOG_PIN("  Pin: ", pin_);
#endif
#endif  // USE_ESP8266

#ifdef USE_ESP32
  LOG_PIN("  Pin: ", pin_);
  if (autorange_) {
    ESP_LOGCONFIG(TAG, " Attenuation: auto");
  } else {
    switch (this->attenuation_) {
      case ADC_ATTEN_DB_0:
        ESP_LOGCONFIG(TAG, " Attenuation: 0db");
        break;
      case ADC_ATTEN_DB_2_5:
        ESP_LOGCONFIG(TAG, " Attenuation: 2.5db");
        break;
      case ADC_ATTEN_DB_6:
        ESP_LOGCONFIG(TAG, " Attenuation: 6db");
        break;
      case ADC_ATTEN_DB_11:
        ESP_LOGCONFIG(TAG, " Attenuation: 11db");
        break;
      default:  // This is to satisfy the unused ADC_ATTEN_MAX
        break;
    }
  }
#endif  // USE_ESP32
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::get_setup_priority() const { return setup_priority::DATA; }
void ADCSensor::update() {
  float value_v = this->sample();
  ESP_LOGV(TAG, "'%s': Got voltage=%.4fV", this->get_name().c_str(), value_v);
  this->publish_state(value_v);
}

#ifdef USE_ESP8266
float ADCSensor::sample() {
#ifdef USE_ADC_SENSOR_VCC
  int raw = ESP.getVcc();  // NOLINT(readability-static-accessed-through-instance)
#else
  int raw = analogRead(this->pin_->get_pin());  // NOLINT
#endif
  if (output_raw_) {
    return raw;
  }
  return raw / 1024.0f;
}
#endif

#ifdef USE_ESP32
esp_err_t get_adc_raw(const adc1_channel_t &channel1, const adc2_channel_t &channel2, ADCSensor::Channel channel_num,
                      int &raw) {
  if (channel_num == ADCSensor::Channel::channel1) {
    raw = adc1_get_raw(channel1);
    return (raw != -1) ? ESP_OK : ESP_FAIL;
  }
  return adc2_get_raw(channel2, ADC_WIDTH_MAX_SOC_BITS, &raw);
}

float ADCSensor::sample() {
  if (!autorange_) {
    int raw;
    esp_err_t ret = get_adc_raw(channel1_, channel2_, channel_num_, raw);
    if (ret != ESP_OK) {
      return NAN;
    }
    if (output_raw_) {
      return raw;
    }
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &cal_characteristics_[(int) attenuation_]);
    return mv / 1000.0f;
  }

  int raw11, raw6 = ADC_MAX, raw2 = ADC_MAX, raw0 = ADC_MAX;
  config_channel_atten(channel1_, channel2_, ADC_ATTEN_DB_11, channel_num_);
  esp_err_t ret = get_adc_raw(channel1_, channel2_, channel_num_, raw11);
  if (raw11 < ADC_MAX && ret == ESP_OK) {
    config_channel_atten(channel1_, channel2_, ADC_ATTEN_DB_6, channel_num_);
    ret = get_adc_raw(channel1_, channel2_, channel_num_, raw6);
    if (raw6 < ADC_MAX && ret == ESP_OK) {
      config_channel_atten(channel1_, channel2_, ADC_ATTEN_DB_2_5, channel_num_);
      ret = get_adc_raw(channel1_, channel2_, channel_num_, raw2);
      if (raw2 < ADC_MAX && ret == ESP_OK) {
        config_channel_atten(channel1_, channel2_, ADC_ATTEN_DB_0, channel_num_);
        ret = get_adc_raw(channel1_, channel2_, channel_num_, raw0);
      }
    }
  }
  if (ret != ESP_OK) {
    return NAN;
  }

  uint32_t mv11 = esp_adc_cal_raw_to_voltage(raw11, &cal_characteristics_[(int) ADC_ATTEN_DB_11]);
  uint32_t mv6 = esp_adc_cal_raw_to_voltage(raw6, &cal_characteristics_[(int) ADC_ATTEN_DB_6]);
  uint32_t mv2 = esp_adc_cal_raw_to_voltage(raw2, &cal_characteristics_[(int) ADC_ATTEN_DB_2_5]);
  uint32_t mv0 = esp_adc_cal_raw_to_voltage(raw0, &cal_characteristics_[(int) ADC_ATTEN_DB_0]);

  // Contribution of each value, in range 0-2048 (12 bit ADC) or 0-4096 (13 bit ADC)
  uint32_t c11 = std::min(raw11, ADC_HALF);
  uint32_t c6 = ADC_HALF - std::abs(raw6 - ADC_HALF);
  uint32_t c2 = ADC_HALF - std::abs(raw2 - ADC_HALF);
  uint32_t c0 = std::min(ADC_MAX - raw0, ADC_HALF);
  // max theoretical csum value is 4096*4 = 16384
  uint32_t csum = c11 + c6 + c2 + c0;

  // each mv is max 3900; so max value is 3900*4096*4, fits in unsigned32
  uint32_t mv_scaled = (mv11 * c11) + (mv6 * c6) + (mv2 * c2) + (mv0 * c0);
  return mv_scaled / (float) (csum * 1000U);
}
#endif  // USE_ESP32

#ifdef USE_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
