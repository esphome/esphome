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

#ifdef USE_ESP32
inline adc1_channel_t gpio_to_adc1(uint8_t pin) {
#if CONFIG_IDF_TARGET_ESP32
  switch (pin) {
    case 36:
      return ADC1_CHANNEL_0;
    case 37:
      return ADC1_CHANNEL_1;
    case 38:
      return ADC1_CHANNEL_2;
    case 39:
      return ADC1_CHANNEL_3;
    case 32:
      return ADC1_CHANNEL_4;
    case 33:
      return ADC1_CHANNEL_5;
    case 34:
      return ADC1_CHANNEL_6;
    case 35:
      return ADC1_CHANNEL_7;
    default:
      return ADC1_CHANNEL_MAX;
  }
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
  switch (pin) {
    case 0:
      return ADC1_CHANNEL_0;
    case 1:
      return ADC1_CHANNEL_1;
    case 2:
      return ADC1_CHANNEL_2;
    case 3:
      return ADC1_CHANNEL_3;
    case 4:
      return ADC1_CHANNEL_4;
    default:
      return ADC1_CHANNEL_MAX;
  }
#endif
}
void ADCSensor::set_attenuation(adc_atten_t attenuation) { this->attenuation_ = attenuation; }
void ADCSensor::set_autorange(bool autorange) { this->autorange_ = autorange; }
#endif

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#ifndef USE_ADC_SENSOR_VCC
  pin_->setup();
#endif

#ifdef USE_ESP32
  if (this->autorange_)
    this->attenuation_ = ADC_ATTEN_DB_11;
  adc1_config_channel_atten(gpio_to_adc1(pin_->get_pin()), attenuation_);
  adc1_config_width(ADC_WIDTH_BIT_12);
#if !CONFIG_IDF_TARGET_ESP32C3 && !CONFIG_IDF_TARGET_ESP32H2
  adc_gpio_init(ADC_UNIT_1, (adc_channel_t) gpio_to_adc1(pin_->get_pin()));
#endif
#endif
}
void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
#ifdef USE_ESP8266
#ifdef USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else
  LOG_PIN("  Pin: ", pin_);
#endif
#endif
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
#endif
  LOG_UPDATE_INTERVAL(this);
}
float ADCSensor::get_setup_priority() const { return setup_priority::DATA; }
void ADCSensor::update() {
  float value_v = this->sample();
  ESP_LOGD(TAG, "'%s': Got voltage=%.2fV", this->get_name().c_str(), value_v);
  this->publish_state(value_v);
}
uint16_t ADCSensor::read_raw_() {
#ifdef USE_ESP32
  return adc1_get_raw(gpio_to_adc1(pin_->get_pin()));
#endif

#ifdef USE_ESP8266
#ifdef USE_ADC_SENSOR_VCC
  return ESP.getVcc();  // NOLINT(readability-static-accessed-through-instance)
#else
  return analogRead(this->pin_->get_pin());  // NOLINT
#endif
#endif
}
uint32_t ADCSensor::raw_to_microvolts_(uint16_t raw) {
#ifdef USE_ESP32
#if CONFIG_IDF_TARGET_ESP32
  switch (this->attenuation_) {
    case ADC_ATTEN_DB_0:
      return raw * 269;  // 1e6 * 1.1 / 4095
    case ADC_ATTEN_DB_2_5:
      return raw * 366;  // 1e6 * 1.5 / 4095
    case ADC_ATTEN_DB_6:
      return raw * 537;  // 1e6 * 2.2 / 4095
    case ADC_ATTEN_DB_11:
      return raw * 952;  // 1e6 * 3.9 / 4095
    default:             // This is to satisfy the unused ADC_ATTEN_MAX
      return raw * 244;  // 1e6 * 1.0 / 4095
  }
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
  switch (this->attenuation_) {
    case ADC_ATTEN_DB_0:
      return raw * 205;  // 1e6 * 0.84 / 4095
    case ADC_ATTEN_DB_2_5:
      return raw * 276;  // 1e6 * 1.13 / 4095
    case ADC_ATTEN_DB_6:
      return raw * 381;  // 1e6 * 1.56 / 4095
    case ADC_ATTEN_DB_11:
      return raw * 733;  // 1e6 * 3.0 / 4095
    default:             // This is to satisfy the unused ADC_ATTEN_MAX
      return raw * 244;  // 1e6 * 1.0 / 4095
  }
#endif
#endif

#ifdef USE_ESP8266
  return raw * 977;  // 1e6 / 1024
#endif
}
float ADCSensor::sample() {
  int raw = this->read_raw_();
  uint32_t v = this->raw_to_microvolts_(raw);
#ifdef USE_ESP32
  if (autorange_) {
    int raw11 = raw, raw6 = 4095, raw2 = 4095, raw0 = 4095;
    uint32_t v11 = v, v6 = 0, v2 = 0, v0 = 0;
    if (raw11 < 4095) {  // Progressively read all attenuation ranges
      adc1_config_channel_atten(gpio_to_adc1(pin_->get_pin()), ADC_ATTEN_DB_6);
      raw6 = this->read_raw_();
      v6 = this->raw_to_microvolts_(raw6);
      if (raw6 < 4095) {
        adc1_config_channel_atten(gpio_to_adc1(pin_->get_pin()), ADC_ATTEN_DB_2_5);
        raw2 = this->read_raw_();
        v2 = this->raw_to_microvolts_(raw2);
        if (raw2 < 4095) {
          adc1_config_channel_atten(gpio_to_adc1(pin_->get_pin()), ADC_ATTEN_DB_0);
          raw0 = this->read_raw_();
          v0 = this->raw_to_microvolts_(raw0);
        }
      }
      adc1_config_channel_atten(gpio_to_adc1(pin_->get_pin()), ADC_ATTEN_DB_11);
    }                                           // Contribution coefficients (normalized to 2048)
    uint16_t c11 = clamp(raw11, 0, 2048);       // high 1, middle 1, low 0
    uint16_t c6 = (2048 - abs(raw6 - 2048));    // high 0, middle 1, low 0
    uint16_t c2 = (2048 - abs(raw2 - 2048));    // high 0, middle 1, low 0
    uint16_t c0 = clamp(4095 - raw0, 0, 2048);  // high 0, middle 1, low 1
    uint32_t csum = c11 + c6 + c2 + c0;         // sum to normalize the final result
    if (csum > 0)
      v = (v11 * c11) + (v6 * c6) + (v2 * c2) + (v0 * c0);
    else
      csum = 1;   // in case of error, keep the 11db output
    csum *= 1e6;  // include the microvolts->volts conversion factor
    return (float) v / (float) csum;  // Normalize & convert
  }
#endif
  return v / (float) 1e6;  // Convert from microvolts to volts
}
#ifdef USE_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
