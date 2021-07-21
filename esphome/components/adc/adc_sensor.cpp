#include "adc_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ADC_SENSOR_VCC
ADC_MODE(ADC_VCC)
#endif

namespace esphome {
namespace adc {

static const char *const TAG = "adc";

#ifdef ARDUINO_ARCH_ESP32
void ADCSensor::set_attenuation(adc_atten_t attenuation) { this->attenuation_ = attenuation; }

inline adc1_channel_t gpio_to_adc1(uint8_t pin) {
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
}
#endif

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#ifndef USE_ADC_SENSOR_VCC
  GPIOPin(this->pin_, INPUT).setup();
#endif

#ifdef ARDUINO_ARCH_ESP32
  adc1_config_channel_atten(gpio_to_adc1(pin_), attenuation_);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc_gpio_init(ADC_UNIT_1, (adc_channel_t) gpio_to_adc1(pin_));
#endif
}
void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
#ifdef ARDUINO_ARCH_ESP8266
#ifdef USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
#endif
#endif
#ifdef ARDUINO_ARCH_ESP32
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
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
float ADCSensor::sample() {
#ifdef ARDUINO_ARCH_ESP32
  int raw = adc1_get_raw(gpio_to_adc1(pin_));
  float value_v = raw / 4095.0f;
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
  return value_v;
#endif

#ifdef ARDUINO_ARCH_ESP8266
#ifdef USE_ADC_SENSOR_VCC
  return ESP.getVcc() / 1024.0f;
#else
  return analogRead(this->pin_) / 1024.0f;  // NOLINT
#endif
#endif
}
#ifdef ARDUINO_ARCH_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
