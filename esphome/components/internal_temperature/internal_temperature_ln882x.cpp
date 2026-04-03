#ifdef USE_LN882X

#include "esphome/core/log.h"
#include "internal_temperature.h"

extern "C" {
uint16_t hal_adc_get_data(uint32_t adc_base, uint32_t ch);
}

namespace esphome {
namespace internal_temperature {

void InternalTemperatureSensor::update() {
  // Read from internal temperature sensor via ADC channel 0.
  // ADC_BASE = 0x40000800, ADC_CH0 = 1 (bit 0 set).
  static constexpr float ADC_TEMP_SCALE = 2.54f;
  static constexpr float ADC_TEMP_OFFSET = 278.15f;
  uint16_t raw = hal_adc_get_data(0x40000800U, 1U);
  float temperature = (raw & 0xFFF) / ADC_TEMP_SCALE - ADC_TEMP_OFFSET;
  this->publish_state(temperature);
}

}  // namespace internal_temperature
}  // namespace esphome

#endif  // USE_LN882X
