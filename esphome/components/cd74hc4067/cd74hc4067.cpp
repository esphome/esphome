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
  static int mux_channel[16][4]= {
    {0,0,0,0}, //channel 0
    {1,0,0,0}, //channel 1
    {0,1,0,0}, //channel 2
    {1,1,0,0}, //channel 3
    {0,0,1,0}, //channel 4
    {1,0,1,0}, //channel 5
    {0,1,1,0}, //channel 6
    {1,1,1,0}, //channel 7
    {0,0,0,1}, //channel 8
    {1,0,0,1}, //channel 9
    {0,1,0,1}, //channel 10
    {1,1,0,1}, //channel 11
    {0,0,1,1}, //channel 12
    {1,0,1,1}, //channel 13
    {0,1,1,1}, //channel 14
    {1,1,1,1}  //channel 15
  };
   this->s0_pin_->digital_write(mux_channel[pin][0]);
   this->s1_pin_->digital_write(mux_channel[pin][1]);
   this->s2_pin_->digital_write(mux_channel[pin][2]);
   this->s3_pin_->digital_write(mux_channel[pin][3]);
   static int num_samples = 1000;
   float sum_squares = 0;
   for (int i = 0; i < num_samples; ++i)
   {
     float value = analogRead(this->adc_pin_); //NO_LINT
     sum_squares += value * value;
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
  return get_mac_address() + "-" + to_string(pin_); 
}

void CD74HC4067Sensor::dump_config() {
  LOG_SENSOR("", "Multiplexer ADC Sensor", this);
  ESP_LOGCONFIG(TAG, "CD74HC4067 Pin: %u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}


}  // namespace cd74hc4067
}  // namespace esphome
