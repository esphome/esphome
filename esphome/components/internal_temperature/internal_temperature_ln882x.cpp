#ifdef USE_LN882X

#include "esphome/core/log.h"
#include "internal_temperature.h"

extern "C" {
uint16_t hal_adc_get_data(uint32_t adc_base, uint32_t ch);
}

namespace esphome {
namespace internal_temperature {

static const char *const TAG = "internal_temperature.ln882x";

void InternalTemperatureSensor::update() {
  // Read from internal temperature sensor via ADC channel 0.
  // ADC_BASE = 0x40000800, ADC_CH0 = 1 (bit 0 set).
  uint16_t raw = hal_adc_get_data(0x40000800U, 1U);
  float temperature = 25.0f + ((raw & 0xFFF) - 770) / 2.54f;

  if (std::isfinite(temperature)) {
    this->publish_state(temperature);
  } else {
    ESP_LOGD(TAG, "Ignoring invalid temperature (value=%.1f)", temperature);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
  }
}

}  // namespace internal_temperature
}  // namespace esphome

#endif  // USE_LN882X
