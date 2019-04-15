#include "esphome/components/adc/adc_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc {

static const char *TAG = "adc";

ADCSensor::ADCSensor(const std::string &name, uint8_t pin, uint32_t update_interval)
    : PollingSensorComponent(name, update_interval), pin_(pin) {}

#ifdef ARDUINO_ARCH_ESP32
void ADCSensor::set_attenuation(adc_attenuation_t attenuation) { this->attenuation_ = attenuation; }
#endif

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
  GPIOPin(this->pin_, INPUT).setup();

#ifdef ARDUINO_ARCH_ESP32
  analogSetPinAttenuation(this->pin_, this->attenuation_);
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
    case ADC_0db:
      ESP_LOGCONFIG(TAG, " Attenuation: 0db (max 1.1V)");
      break;
    case ADC_2_5db:
      ESP_LOGCONFIG(TAG, " Attenuation: 2.5db (max 1.5V)");
      break;
    case ADC_6db:
      ESP_LOGCONFIG(TAG, " Attenuation: 6db (max 2.2V)");
      break;
    case ADC_11db:
      ESP_LOGCONFIG(TAG, " Attenuation: 11db (max 3.9V)");
      break;
  }
#endif
  LOG_UPDATE_INTERVAL(this);
}
float ADCSensor::get_setup_priority() const { return setup_priority::DATA; }
void ADCSensor::update() {
#ifdef ARDUINO_ARCH_ESP32
  float value_v = analogRead(this->pin_) / 4095.0f;
  switch (this->attenuation_) {
    case ADC_0db:
      value_v *= 1.1;
      break;
    case ADC_2_5db:
      value_v *= 1.5;
      break;
    case ADC_6db:
      value_v *= 2.2;
      break;
    case ADC_11db:
      value_v *= 3.9;
      break;
  }
#endif

#ifdef ARDUINO_ARCH_ESP8266
#ifdef USE_ADC_SENSOR_VCC
  float value_v = ESP.getVcc() / 1024.0f;
#else
  float value_v = analogRead(this->pin_) / 1024.0f;
#endif
#endif

  ESP_LOGD(TAG, "'%s': Got voltage=%.2fV", this->get_name().c_str(), value_v);

  this->publish_state(value_v);
}
#ifdef ARDUINO_ARCH_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
