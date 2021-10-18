#include "adc_sensor.h"
#include "esphome/core/log.h"

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
#define ADC_MAX 4095
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
void ADCSensor::set_attenuation(adc_atten_t attenuation) {
  this->attenuation_ = attenuation;
  adc1_config_channel_atten(gpio_to_adc1(pin_->get_pin()), attenuation_);
}
#endif

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#ifndef USE_ADC_SENSOR_VCC
  pin_->setup();
#endif

#ifdef USE_ESP32
#ifdef USE_ADC_AUTORANGE
  auto_range_ = true;
#endif
  if (auto_range_)
    attenuation_ = ADC_ATTEN_DB_11;
  this->set_attenuation(attenuation_);
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
  if (auto_range_)
    ESP_LOGCONFIG(TAG, " Attenuation: auto-range");
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
int ADCSensor::read_raw_() {
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
float ADCSensor::raw_to_voltage_(int raw) {
#ifdef USE_ESP32
  float value_v = raw / (float) ADC_MAX;
#if CONFIG_IDF_TARGET_ESP32
  switch (this->attenuation_) {
    case ADC_ATTEN_DB_0:
      value_v *= 1.1;
      break;
    case ADC_ATTEN_DB_2_5:
      value_v *= 1.5;
      break;
    case ADC_ATTEN_DB_6:
      value_v *= 2.2;
      break;
    case ADC_ATTEN_DB_11:
      value_v *= 3.9;
      break;
    default:  // This is to satisfy the unused ADC_ATTEN_MAX
      break;
  }
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
  switch (this->attenuation_) {
    case ADC_ATTEN_DB_0:
      value_v *= 0.84;
      break;
    case ADC_ATTEN_DB_2_5:
      value_v *= 1.13;
      break;
    case ADC_ATTEN_DB_6:
      value_v *= 1.56;
      break;
    case ADC_ATTEN_DB_11:
      value_v *= 3.0;
      break;
    default:  // This is to satisfy the unused ADC_ATTEN_MAX
      break;
  }
#endif
  return value_v;
#endif

#ifdef USE_ESP8266
  return raw / 1024.0f;
#endif
}
float ADCSensor::sample() {
  int raw = this->read_raw_();
  float value_v = this->raw_to_voltage_(raw);
#ifdef USE_ESP32
  if (auto_range_) {
    int raw11 = raw;
    int raw6 = ADC_MAX;
    int raw2 = ADC_MAX;
    int raw0 = ADC_MAX;
    if (raw11 < ADC_MAX) {  // Progressively read all attenuation ranges
      this->set_attenuation(ADC_ATTEN_DB_6);
      raw6 = this->read_raw_();
      if (raw6 < ADC_MAX) {
        this->set_attenuation(ADC_ATTEN_DB_2_5);
        raw2 = this->read_raw_();
        if (raw2 < ADC_MAX) {
          this->set_attenuation(ADC_ATTEN_DB_0);
          raw0 = this->read_raw_();
        }
      }
      this->set_attenuation(ADC_ATTEN_DB_11);
    }
    // TO-DO: Cleanup the next part
    float c11 = (float)(2048-abs(raw11-2048)) / 2048.f;  // Triangular coefficient ponderation. 1 at middle, 0 at limits.
    float c6 = (float)(2048-abs(raw6-2048)) / 2048.f;
    float c2 = (float)(2048-abs(raw2-2048)) / 2048.f;
    float c0 = (float)(2048-abs(raw0-2048)) / 2048.f;
    float csum = c11 + c6 + c2 + c0;  // To normalize the result
    if (csum > 0)
      value_v = (this->raw_to_voltage(raw11)*c11 + this->raw_to_voltage(raw6)*c6 + this->raw_to_voltage(raw2)*c2 + this->raw_to_voltage(raw0)*c0) / csum;
  }
#endif
  return value_v;
}
#ifdef USE_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
