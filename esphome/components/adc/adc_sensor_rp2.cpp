#ifdef USE_RP2

#include "adc_sensor.h"
#include "esphome/core/log.h"

#ifdef CYW43_USES_VSYS_PIN
#include "pico/cyw43_arch.h"
#endif  // CYW43_USES_VSYS_PIN
#include <hardware/adc.h>

// PICO_VSYS_PIN is defined in pico-sdk board headers (e.g. boards/pico2.h),
// but the Arduino framework's config_autogen.h includes a generic board header
// that doesn't define it. Provide the standard value (pin 29) as a fallback.
#ifndef PICO_VSYS_PIN
#define PICO_VSYS_PIN 29  // NOLINT(cppcoreguidelines-macro-usage)
#endif

namespace esphome::adc {

static const char *const TAG = "adc";

// The on-die temperature sensor sits on the last ADC channel: input 4 on RP2040
// and RP2350A, but input 8 on RP2350B, which has eight external channels rather
// than four.
//
// This deliberately does not use the SDK's ADC_TEMPERATURE_CHANNEL_NUM. That
// derives from NUM_ADC_CHANNELS, which <pico.h> settles from a board header, and
// arduino-pico supplies a fixed B-die one for every RP2350 build. The real die
// is only declared later, by the variant's pins_arduino.h, so the SDK constant
// reads 8 on A-die boards. PICO_RP2350A itself is correct by the time this file
// is compiled, on both arduino-pico and pico-sdk builds.
#if defined(PICO_RP2350) && !defined(PICO_RP2350A)
#error "PICO_RP2350A is not defined, so the RP2350 die is unknown and the temperature ADC channel cannot be chosen"
#endif
#if defined(PICO_RP2350) && !PICO_RP2350A
static constexpr uint8_t TEMPERATURE_ADC_INPUT = 8;
#else
static constexpr uint8_t TEMPERATURE_ADC_INPUT = 4;
#endif

void ADCSensor::setup() {
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
  ESP_LOGCONFIG(TAG,
                "  Samples: %i\n"
                "  Sampling mode: %s",
                this->sample_count_, LOG_STR_ARG(sampling_mode_to_str(this->sampling_mode_)));
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  uint32_t raw = 0;
  auto aggr = Aggregator<uint32_t>(this->sampling_mode_);

  if (this->is_temperature_) {
    adc_set_temp_sensor_enabled(true);
    delay(1);
    adc_select_input(TEMPERATURE_ADC_INPUT);

    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      raw = adc_read();
      aggr.add_sample(raw);
    }
    adc_set_temp_sensor_enabled(false);
    if (this->output_raw_) {
      return aggr.aggregate();
    }
    return aggr.aggregate() * 3.3f / 4096.0f;
  }

  uint8_t pin = this->pin_->get_pin();
#if defined(CYW43_USES_VSYS_PIN) && defined(USE_WIFI)
  if (pin == PICO_VSYS_PIN) {
    // Measuring VSYS on Raspberry Pico W needs to be wrapped with
    // `cyw43_thread_enter()`/`cyw43_thread_exit()` as discussed in
    // https://github.com/raspberrypi/pico-sdk/issues/1222, since Wifi chip and
    // VSYS ADC both share GPIO29.
    // The USE_WIFI guard is required because CYW43_USES_VSYS_PIN can be defined
    // transitively (e.g. via lwip_wrap.h) even on non-WiFi boards where the CYW43
    // driver is never initialized; calling cyw43_thread_enter() there hard-faults.
    cyw43_thread_enter();
  }
#endif  // defined(CYW43_USES_VSYS_PIN) && defined(USE_WIFI)

  adc_gpio_init(pin);
  adc_select_input(pin - 26);

  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
    raw = adc_read();
    aggr.add_sample(raw);
  }

#if defined(CYW43_USES_VSYS_PIN) && defined(USE_WIFI)
  if (pin == PICO_VSYS_PIN) {
    cyw43_thread_exit();
  }
#endif  // defined(CYW43_USES_VSYS_PIN) && defined(USE_WIFI)

  if (this->output_raw_) {
    return aggr.aggregate();
  }
  float coeff = pin == PICO_VSYS_PIN ? 3.0f : 1.0f;
  return aggr.aggregate() * 3.3f / 4096.0f * coeff;
}

}  // namespace esphome::adc

#endif  // USE_RP2
