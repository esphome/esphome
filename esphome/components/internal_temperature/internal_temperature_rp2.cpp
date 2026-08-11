#ifdef USE_RP2

#include "esphome/core/log.h"
#include "internal_temperature.h"

#include <cmath>
#include <hardware/adc.h>

// The RP2 variant headers (pulled in transitively by Arduino.h) define
// ADC_RESOLUTION as the pin-level ADC bit count, which would be substituted
// into the constant below. Nothing here uses the Arduino definition, so drop
// it for this file. Not restored with pop_macro: the uses below would then be
// substituted again.
#undef ADC_RESOLUTION

namespace esphome::internal_temperature {

static const char *const TAG = "internal_temperature.rp2";

// ADC input 4 is wired to the on-die temperature sensor on both RP2040 and
// RP2350. The ADC returns a raw conversion, so the datasheet formula below
// turns it into degrees Celsius.
static constexpr uint8_t TEMPERATURE_ADC_INPUT = 4;
static constexpr float ADC_VREF = 3.3f;
static constexpr float ADC_RESOLUTION = 4096.0f;  // 12-bit
// RP2040 datasheet 4.9.5 / RP2350 datasheet 12.4.6: T = 27 - (V - 0.706) / 0.001721
static constexpr float TEMPERATURE_AT_REFERENCE = 27.0f;
static constexpr float REFERENCE_VOLTAGE = 0.706f;
static constexpr float VOLTS_PER_DEGREE = 0.001721f;

static float read_internal_temperature() {
  // adc_init() resets the ADC block, so only do it if nothing else has. The
  // adc component calls it from its own setup(); both re-select their input
  // on every read, so the ordering between them does not matter.
  static bool adc_ready = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  if (!adc_ready) {
    adc_init();
    adc_ready = true;
  }

  adc_set_temp_sensor_enabled(true);
  adc_select_input(TEMPERATURE_ADC_INPUT);
  const uint16_t raw = adc_read();
  adc_set_temp_sensor_enabled(false);

  const float voltage = raw * (ADC_VREF / ADC_RESOLUTION);
  return TEMPERATURE_AT_REFERENCE - (voltage - REFERENCE_VOLTAGE) / VOLTS_PER_DEGREE;
}

void InternalTemperatureSensor::update() {
  float temperature = NAN;
  bool success = false;

  temperature = read_internal_temperature();
  success = (temperature != 0.0f);

  if (success && std::isfinite(temperature)) {
    this->publish_state(temperature);
  } else {
    ESP_LOGD(TAG, "Ignoring invalid temperature (success=%d, value=%.1f)", success, temperature);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
  }
}

}  // namespace esphome::internal_temperature

#endif  // USE_RP2
