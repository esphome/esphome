#ifdef USE_LN882X

#include "internal_temperature.h"

extern "C" {
uint16_t hal_adc_get_data(uint32_t adc_base, uint32_t ch);
}

namespace esphome::internal_temperature {

void InternalTemperatureSensor::update() {
  static constexpr uint32_t ADC_BASE = 0x40000800U;
  static constexpr uint32_t ADC_CH0 = 1U;
  static constexpr uint16_t ADC_MASK = 0xFFF;
  static constexpr float ADC_TEMP_SCALE = 2.54f;
  static constexpr float ADC_TEMP_OFFSET = 278.15f;
  uint16_t raw = hal_adc_get_data(ADC_BASE, ADC_CH0);
  float temperature = (raw & ADC_MASK) / ADC_TEMP_SCALE - ADC_TEMP_OFFSET;
  this->publish_state(temperature);
}

}  // namespace esphome::internal_temperature

#endif  // USE_LN882X
