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

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#ifndef USE_ADC_SENSOR_VCC
  pin_->setup();
#endif

#ifdef USE_ESP32
  adc1_config_width(ADC_WIDTH_BIT_12);
  if (!autorange_) {
    adc1_config_channel_atten(channel_, attenuation_);
  }

  // load characteristics for each attenuation
  for (int i = 0; i < (int) ADC_ATTEN_MAX; i++) {
    auto cal_value = esp_adc_cal_characterize(ADC_UNIT_1, (adc_atten_t) i, ADC_WIDTH_BIT_12,
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

  // adc_gpio_init doesn't exist on ESP32-C3 or ESP32-H2
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32H2)
  adc_gpio_init(ADC_UNIT_1, (adc_channel_t) channel_);
#endif
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
  if (autorange_)
    ESP_LOGCONFIG(TAG, " Attenuation: auto");
  else
    switch (this->attenuation_) {
      case ADC_ATTEN_DB_0:
        ESP_LOGCONFIG(TAG, " Attenuation: 0db (max 1.1V)");
        break;
      case ADC_ATTEN_DB_2_5:
        ESP_LOGCONFIG(TAG, " Attenuation: 2.5db (max 1.5V)");
        break;
      case ADC_ATTEN_DB_6:
        ESP_LOGCONFIG(TAG, " Attenuation: 6db (max 2.2V)");
        break;
      case ADC_ATTEN_DB_11:
        ESP_LOGCONFIG(TAG, " Attenuation: 11db (max 3.9V)");
        break;
      default:  // This is to satisfy the unused ADC_ATTEN_MAX
        break;
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
float ADCSensor::sample() {
  if (!autorange_) {
    int raw = adc1_get_raw(channel_);
    if (raw == -1) {
      return NAN;
    }
    if (output_raw_) {
      return raw;
    }
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &cal_characteristics_[(int) attenuation_]);
    return mv / 1000.0f;
  }

  int raw11, raw6 = 4095, raw2 = 4095, raw0 = 4095;
  adc1_config_channel_atten(channel_, ADC_ATTEN_DB_11);
  raw11 = adc1_get_raw(channel_);
  if (raw11 < 4095) {
    adc1_config_channel_atten(channel_, ADC_ATTEN_DB_6);
    raw6 = adc1_get_raw(channel_);
    if (raw6 < 4095) {
      adc1_config_channel_atten(channel_, ADC_ATTEN_DB_2_5);
      raw2 = adc1_get_raw(channel_);
      if (raw2 < 4095) {
        adc1_config_channel_atten(channel_, ADC_ATTEN_DB_0);
        raw0 = adc1_get_raw(channel_);
      }
    }
  }

  if (raw0 == -1 || raw2 == -1 || raw6 == -1 || raw11 == -1) {
    return NAN;
  }

  uint32_t mv11 = esp_adc_cal_raw_to_voltage(raw11, &cal_characteristics_[(int) ADC_ATTEN_DB_11]);
  uint32_t mv6 = esp_adc_cal_raw_to_voltage(raw6, &cal_characteristics_[(int) ADC_ATTEN_DB_6]);
  uint32_t mv2 = esp_adc_cal_raw_to_voltage(raw2, &cal_characteristics_[(int) ADC_ATTEN_DB_2_5]);
  uint32_t mv0 = esp_adc_cal_raw_to_voltage(raw0, &cal_characteristics_[(int) ADC_ATTEN_DB_0]);

  // Contribution of each value, in range 0-2048
  uint32_t c11 = std::min(raw11, 2048);
  uint32_t c6 = 2048 - std::abs(raw6 - 2048);
  uint32_t c2 = 2048 - std::abs(raw2 - 2048);
  uint32_t c0 = std::min(4095 - raw0, 2048);
  // max theoretical csum value is 2048*4 = 8192
  uint32_t csum = c11 + c6 + c2 + c0;

  // each mv is max 3900; so max value is 3900*2048*4, fits in unsigned
  uint32_t mv_scaled = (mv11 * c11) + (mv6 * c6) + (mv2 * c2) + (mv0 * c0);
  return mv_scaled / (float) (csum * 1000U);
}
#endif  // USE_ESP32

#ifdef USE_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
