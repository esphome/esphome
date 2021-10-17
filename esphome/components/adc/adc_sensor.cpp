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
void ADCSensor::set_attenuation(adc_atten_t attenuation) {
  this->attenuation_ = attenuation;
  adc1_config_channel_atten(gpio_to_adc1(pin_->get_pin()), attenuation_);
}
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
#endif

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#ifndef USE_ADC_SENSOR_VCC
  pin_->setup();
#endif

#ifdef USE_ESP32
  if (auto_range_)
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
int ADCSensor::read_raw() {
  int raw = 0;
  #ifdef USE_ESP32
    raw = adc1_get_raw(gpio_to_adc1(pin_->get_pin()));
  #endif
  
  #ifdef USE_ESP8266
  #ifdef USE_ADC_SENSOR_VCC
    raw = ESP.getVcc();  // NOLINT(readability-static-accessed-through-instance)
  #else
    raw = analogRead(this->pin_->get_pin());  // NOLINT
  #endif
  #endif
  return raw;
}
float ADCSensor::raw_to_voltage(int raw) {
  float value_v = 0;
#ifdef USE_ESP32
  value_v = raw / 4095.0f;
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
# endif
# endif

#ifdef USE_ESP8266
  value_v = raw / 1024.0f;
#endif
  return value_v;
}
float ADCSensor::sample() {
  int raw = this->read_raw();
  float value_v = this->raw_to_voltage(raw);
#ifdef USE_ESP32
  if (auto_range_) {
    float v_acc = 0;   // Accumulator for valid ADC samples (>0 && <4095)
    int nsamples = 0;  // TO-DO: Work only with the linear range limits?
    if (raw < 4095) {
      if (raw > 0) {
        v_acc += value_v;
        nsamples++;
      }
      this->set_attenuation(ADC_ATTEN_DB_6);
      raw = this->read_raw();
      if (raw < 4095) {
        if (raw > 0) {
          v_acc += this->raw_to_voltage(raw);
          nsamples++;
        }
        this->set_attenuation(ADC_ATTEN_DB_2_5);
        raw = this->read_raw();
        if (raw < 4095) {
          if (raw > 0) {
            v_acc += this->raw_to_voltage(raw);
            nsamples++;
          }
          this->set_attenuation(ADC_ATTEN_DB_0);
          raw = this->read_raw();
          if (raw < 4095) {
            v_acc += this->raw_to_voltage(raw);
            nsamples++;
          }
        }
      }
      value_v = v_acc / (float)nsamples;  // Average the valid samples
    }
    this->set_attenuation(ADC_ATTEN_DB_11);
  }
#endif
  return value_v;
}
#ifdef USE_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
