#include "esphome/core/log.h"
#include "cd74hc4067.h"

namespace esphome {
namespace cd74hc4067 {

static const char *TAG = "cd74hc4067";

float CD74HC4067Component::get_setup_priority() const { return setup_priority::HARDWARE; }

void CD74HC4067Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CD74HC4067...");
  this->s0_pin_->pin_mode(OUTPUT);
  this->s1_pin_->pin_mode(OUTPUT);
  this->s2_pin_->pin_mode(OUTPUT);
  this->s3_pin_->pin_mode(OUTPUT);
  GPIOPin(this->adc_pin_, INPUT).setup();
}
void CD74HC4067Component::dump_config() {
  LOG_SENSOR("", "CD74HC4067 Multiplexer", this);
  ESP_LOGCONFIG(TAG, "  ADC Pin: %u", this->adc_pin_);
  ESP_LOGCONFIG(TAG, "  S0 Pin: %u", this->s0_pin_);
  ESP_LOGCONFIG(TAG, "  S1 Pin: %u", this->s1_pin_);
  ESP_LOGCONFIG(TAG, "  S2 Pin: %u", this->s2_pin_);
  ESP_LOGCONFIG(TAG, "  S3 Pin: %u", this->s3_pin_);
}

float CD74HC4067Component::read_data_(uint8_t pin) {
   this->s0_pin_->digital_write(HIGH && bitRead(pin, 0));
   this->s1_pin_->digital_write(HIGH && bitRead(pin, 1));
   this->s2_pin_->digital_write(HIGH && bitRead(pin, 2));
   this->s3_pin_->digital_write(HIGH && bitRead(pin, 3));
   delay(10);
   static int num_samples = 1000;
   float sum_squares = 0;
   for (int i = 0; i < num_samples; ++i)
   {
     float value = analogRead(this->adc_pin_);
     sum_squares += value * value;
     delay(0.0002);
   }
   float rms = sqrt(sum_squares / num_samples);
 
#ifdef ARDUINO_ARCH_ESP8266
   return rms / 1024.0f;
#endif
#ifdef ARDUINO_ARCH_ESP32
   return rms / 4095.0f;
#endif
}

CD74HC4067Sensor::CD74HC4067Sensor(CD74HC4067Component *parent, std::string name, uint8_t pin)
    : PollingComponent(1000), parent_(parent), pin_(pin) {
  this->set_name(name);
}
void CD74HC4067Sensor::setup() { 
  LOG_SENSOR("", "Setting up CD74HC4067 Multiplexer '%s'...", this);
  }
  
void CD74HC4067Sensor::update() {
  float value_v = this->sample();
  this->publish_state(value_v);
}

float CD74HC4067Sensor::get_setup_priority() const { return setup_priority::DATA; }

float CD74HC4067Sensor::sample() {
  float value_v = this->parent_->read_data_(pin_);
  return value_v;
}

std::string CD74HC4067Sensor::unique_id() { 
  char buffer[3];
  itoa(pin_, buffer, 10);
  return get_mac_address() + "-" + buffer; 
}

void CD74HC4067Sensor::dump_config() {
  LOG_SENSOR("", "Multiplexer ADC Sensor", this);
  ESP_LOGCONFIG(TAG, "CD74HC4067 Pin: %u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}


}  // namespace cd74hc4067
}  // namespace esphome
