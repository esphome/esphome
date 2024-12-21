#ifdef USE_RP2040

#include "adc_sensor.h"
#include "esphome/core/log.h"

#ifdef CYW43_USES_VSYS_PIN
#include "pico/cyw43_arch.h"
#endif  // CYW43_USES_VSYS_PIN
#include <hardware/adc.h>

namespace esphome {
namespace adc {

static const char *const TAG = "adc.rp2040";

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
  static bool initialized = false;
  if (!initialized) {
    adc_init();
    initialized = true;
  }
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
  if (this->is_temperature_) {
    ESP_LOGCONFIG(TAG, "  Pin: Temperature");
  } else {
#ifdef USE_ADC_SENSOR_VCC
    ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else
    LOG_PIN("  Pin: ", this->pin_);
#endif  // USE_ADC_SENSOR_VCC
  }
  ESP_LOGCONFIG(TAG, "  Samples: %i", this->sample_count_);
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  if (this->is_temperature_) {
    adc_set_temp_sensor_enabled(true);
    delay(1);
    adc_select_input(4);
    uint32_t raw = 0;
    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      raw += adc_read();
    }
    raw = (raw + (this->sample_count_ >> 1)) / this->sample_count_;  // NOLINT(clang-analyzer-core.DivideZero)
    adc_set_temp_sensor_enabled(false);
    if (this->output_raw_) {
      return raw;
    }
    return raw * 3.3f / 4096.0f;
  }

  uint8_t pin = this->pin_->get_pin();
#ifdef CYW43_USES_VSYS_PIN
  if (pin == PICO_VSYS_PIN) {
    // Measuring VSYS on Raspberry Pico W needs to be wrapped with
    // `cyw43_thread_enter()`/`cyw43_thread_exit()` as discussed in
    // https://github.com/raspberrypi/pico-sdk/issues/1222, since Wifi chip and
    // VSYS ADC both share GPIO29
    cyw43_thread_enter();
  }
#endif  // CYW43_USES_VSYS_PIN

  adc_gpio_init(pin);
  adc_select_input(pin - 26);

  uint32_t raw = 0;
  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
    raw += adc_read();
  }
  raw = (raw + (this->sample_count_ >> 1)) / this->sample_count_;  // NOLINT(clang-analyzer-core.DivideZero)

#ifdef CYW43_USES_VSYS_PIN
  if (pin == PICO_VSYS_PIN) {
    cyw43_thread_exit();
  }
#endif  // CYW43_USES_VSYS_PIN

  if (this->output_raw_) {
    return raw;
  }
  float coeff = pin == PICO_VSYS_PIN ? 3.0f : 1.0f;
  return raw * 3.3f / 4096.0f * coeff;
}

}  // namespace adc
}  // namespace esphome

#endif  // USE_RP2040
