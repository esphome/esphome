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

#ifdef USE_RP2040
#include <hardware/adc.h>
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
#endif

#ifdef USE_RP2040
extern "C"
#endif
    void
    ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#if !defined(USE_ADC_SENSOR_VCC) && !defined(USE_RP2040)
  pin_->setup();
#endif

#ifdef USE_ESP32
  adc1_config_width(ADC_WIDTH_MAX_SOC_BITS);
  if (!autorange_) {
    adc1_config_channel_atten(channel_, attenuation_);
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

#ifdef USE_RP2040
  static bool initialized = false;
  if (!initialized) {
    adc_init();
    initialized = true;
  }
#endif

  ESP_LOGCONFIG(TAG, "ADC '%s' setup finished!", this->get_name().c_str());
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
#ifdef USE_RP2040
  if (this->is_temperature_) {
    ESP_LOGCONFIG(TAG, "  Pin: Temperature");
  } else {
    LOG_PIN("  Pin: ", pin_);
  }
#endif
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

  int raw11, raw6 = ADC_MAX, raw2 = ADC_MAX, raw0 = ADC_MAX;
  adc1_config_channel_atten(channel_, ADC_ATTEN_DB_11);
  raw11 = adc1_get_raw(channel_);
  if (raw11 < ADC_MAX) {
    adc1_config_channel_atten(channel_, ADC_ATTEN_DB_6);
    raw6 = adc1_get_raw(channel_);
    if (raw6 < ADC_MAX) {
      adc1_config_channel_atten(channel_, ADC_ATTEN_DB_2_5);
      raw2 = adc1_get_raw(channel_);
      if (raw2 < ADC_MAX) {
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

#ifdef USE_RP2040
float ADCSensor::sample() {
  if (this->is_temperature_) {
    adc_set_temp_sensor_enabled(true);
    delay(1);
    adc_select_input(4);
  } else {
    uint8_t pin = this->pin_->get_pin();
    adc_gpio_init(pin);
    adc_select_input(pin - 26);
  }

  int raw = adc_read();
  if (this->is_temperature_) {
    adc_set_temp_sensor_enabled(false);
  }
  if (output_raw_) {
    return raw;
  }
  return raw * 3.3f / 4096.0f;
}
#endif

#ifdef USE_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
