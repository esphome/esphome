#include "waveshare_io_ch32v003_sensor.h"

#include "esphome/core/log.h"

namespace esphome::waveshare_io_ch32v003 {

static const char *const TAG = "waveshare_io_ch32v003.sensor";

float WaveshareIOCH32V003Sensor::get_setup_priority() const { return setup_priority::DATA; }

void WaveshareIOCH32V003Sensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "WaveshareIOCH32V003Sensor:\n"
                "  Reference Voltage: %.2fV",
                this->reference_voltage_);
}

float WaveshareIOCH32V003Sensor::sample() {
  uint16_t adc_value = this->parent_->get_adc_value();
  // Convert the ADC value to voltage. 10-bit ADC
  float voltage = adc_value * this->reference_voltage_ / 1023.0f;
  return voltage;
}

void WaveshareIOCH32V003Sensor::update() { this->publish_state(this->sample()); }

}  // namespace esphome::waveshare_io_ch32v003
