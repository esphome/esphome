#include "adc_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP8266
#ifdef USE_ADC_SENSOR_VCC
#include <Esp.h>
ADC_MODE(ADC_VCC)
#else
#include <Arduino.h>
#endif
#endif

#ifdef USE_RP2040
#ifdef CYW43_USES_VSYS_PIN
#include "pico/cyw43_arch.h"
#endif
#include <hardware/adc.h>
#endif

namespace esphome {
namespace adc {

static const char *const TAG = "adc";

// 13-bit for S2, 12-bit for all other ESP32 variants
#ifdef USE_ESP32
static const adc_bits_width_t ADC_WIDTH_MAX_SOC_BITS = static_cast<adc_bits_width_t>(ADC_WIDTH_MAX - 1);

#ifndef SOC_ADC_RTC_MAX_BITWIDTH
#if USE_ESP32_VARIANT_ESP32S2
static const int32_t SOC_ADC_RTC_MAX_BITWIDTH = 13;
#else
static const int32_t SOC_ADC_RTC_MAX_BITWIDTH = 12;
#endif
#endif

static const int ADC_MAX = (1 << SOC_ADC_RTC_MAX_BITWIDTH) - 1;    // 4095 (12 bit) or 8191 (13 bit)
static const int ADC_HALF = (1 << SOC_ADC_RTC_MAX_BITWIDTH) >> 1;  // 2048 (12 bit) or 4096 (13 bit)
#endif

#ifdef USE_RP2040
extern "C"
#endif
    void
    ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#if !defined(USE_ADC_SENSOR_VCC) && !defined(USE_RP2040)
  this->pin_->setup();
#endif

#ifdef USE_ESP32
  if (this->channel1_ != ADC1_CHANNEL_MAX) {
    adc1_config_width(ADC_WIDTH_MAX_SOC_BITS);
    if (!this->autorange_) {
      adc1_config_channel_atten(this->channel1_, this->attenuation_);
    }
  } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
    if (!this->autorange_) {
      adc2_config_channel_atten(this->channel2_, this->attenuation_);
    }
  }

  // load characteristics for each attenuation
  for (int32_t i = 0; i <= ADC_ATTEN_DB_12_COMPAT; i++) {
    auto adc_unit = this->channel1_ != ADC1_CHANNEL_MAX ? ADC_UNIT_1 : ADC_UNIT_2;
    auto cal_value = esp_adc_cal_characterize(adc_unit, (adc_atten_t) i, ADC_WIDTH_MAX_SOC_BITS,
                                              1100,  // default vref
                                              &this->cal_characteristics_[i]);
    switch (cal_value) {
      case ESP_ADC_CAL_VAL_EFUSE_VREF:
        ESP_LOGV(TAG, "Using eFuse Vref for calibration");
        break;
      case ESP_ADC_CAL_VAL_EFUSE_TP:
        ESP_LOGV(TAG, "Using two-point eFuse Vref for calibration");
        break;
      case ESP_ADC_CAL_VAL_DEFAULT_VREF:
      default:
        break;
    }
  }

#endif  // USE_ESP32

#ifdef USE_RP2040
  static bool initialized = false;
  if (!initialized) {
    adc_init();
    initialized = true;
  }
#endif

  ESP_LOGCONFIG(TAG, "ADC '%s' setup finished!", this->get_name().c_str());
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
#if defined(USE_ESP8266) || defined(USE_LIBRETINY)
#ifdef USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else
  LOG_PIN("  Pin: ", this->pin_);
#endif
#endif  // USE_ESP8266 || USE_LIBRETINY

#ifdef USE_ESP32
  LOG_PIN("  Pin: ", this->pin_);
  if (this->autorange_) {
    ESP_LOGCONFIG(TAG, "  Attenuation: auto");
  } else {
    switch (this->attenuation_) {
      case ADC_ATTEN_DB_0:
        ESP_LOGCONFIG(TAG, "  Attenuation: 0db");
        break;
      case ADC_ATTEN_DB_2_5:
        ESP_LOGCONFIG(TAG, "  Attenuation: 2.5db");
        break;
      case ADC_ATTEN_DB_6:
        ESP_LOGCONFIG(TAG, "  Attenuation: 6db");
        break;
      case ADC_ATTEN_DB_12_COMPAT:
        ESP_LOGCONFIG(TAG, "  Attenuation: 12db");
        break;
      default:  // This is to satisfy the unused ADC_ATTEN_MAX
        break;
    }
  }
#endif  // USE_ESP32

#ifdef USE_RP2040
  if (this->is_temperature_) {
    ESP_LOGCONFIG(TAG, "  Pin: Temperature");
  } else {
#ifdef USE_ADC_SENSOR_VCC
    ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else
    LOG_PIN("  Pin: ", this->pin_);
#endif  // USE_ADC_SENSOR_VCC
  }
#endif  // USE_RP2040
  ESP_LOGCONFIG(TAG, "  Samples: %i", this->sample_count_);
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::get_setup_priority() const { return setup_priority::DATA; }
void ADCSensor::update() {
  float value_v = this->sample();
  ESP_LOGV(TAG, "'%s': Got voltage=%.4fV", this->get_name().c_str(), value_v);
  this->publish_state(value_v);
}

void ADCSensor::set_sample_count(uint8_t sample_count) {
  if (sample_count != 0) {
    this->sample_count_ = sample_count;
  }
}

#ifdef USE_ESP8266
float ADCSensor::sample() {
  uint32_t raw = 0;
  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
#ifdef USE_ADC_SENSOR_VCC
    raw += ESP.getVcc();  // NOLINT(readability-static-accessed-through-instance)
#else
    raw += analogRead(this->pin_->get_pin());  // NOLINT
#endif
  }
  raw = (raw + (this->sample_count_ >> 1)) / this->sample_count_;  // NOLINT(clang-analyzer-core.DivideZero)
  if (this->output_raw_) {
    return raw;
  }
  return raw / 1024.0f;
}
#endif

#ifdef USE_ESP32
float ADCSensor::sample() {
  if (!this->autorange_) {
    uint32_t sum = 0;
    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      int raw = -1;
      if (this->channel1_ != ADC1_CHANNEL_MAX) {
        raw = adc1_get_raw(this->channel1_);
      } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
        adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw);
      }
      if (raw == -1) {
        return NAN;
      }
      sum += raw;
    }
    sum = (sum + (this->sample_count_ >> 1)) / this->sample_count_;  // NOLINT(clang-analyzer-core.DivideZero)
    if (this->output_raw_) {
      return sum;
    }
    uint32_t mv = esp_adc_cal_raw_to_voltage(sum, &this->cal_characteristics_[(int32_t) this->attenuation_]);
    return mv / 1000.0f;
  }

  int raw12 = ADC_MAX, raw6 = ADC_MAX, raw2 = ADC_MAX, raw0 = ADC_MAX;

  if (this->channel1_ != ADC1_CHANNEL_MAX) {
    adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_12_COMPAT);
    raw12 = adc1_get_raw(this->channel1_);
    if (raw12 < ADC_MAX) {
      adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_6);
      raw6 = adc1_get_raw(this->channel1_);
      if (raw6 < ADC_MAX) {
        adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_2_5);
        raw2 = adc1_get_raw(this->channel1_);
        if (raw2 < ADC_MAX) {
          adc1_config_channel_atten(this->channel1_, ADC_ATTEN_DB_0);
          raw0 = adc1_get_raw(this->channel1_);
        }
      }
    }
  } else if (this->channel2_ != ADC2_CHANNEL_MAX) {
    adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_12_COMPAT);
    adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw12);
    if (raw12 < ADC_MAX) {
      adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_6);
      adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw6);
      if (raw6 < ADC_MAX) {
        adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_2_5);
        adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw2);
        if (raw2 < ADC_MAX) {
          adc2_config_channel_atten(this->channel2_, ADC_ATTEN_DB_0);
          adc2_get_raw(this->channel2_, ADC_WIDTH_MAX_SOC_BITS, &raw0);
        }
      }
    }
  }

  if (raw0 == -1 || raw2 == -1 || raw6 == -1 || raw12 == -1) {
    return NAN;
  }

  uint32_t mv12 = esp_adc_cal_raw_to_voltage(raw12, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_12_COMPAT]);
  uint32_t mv6 = esp_adc_cal_raw_to_voltage(raw6, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_6]);
  uint32_t mv2 = esp_adc_cal_raw_to_voltage(raw2, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_2_5]);
  uint32_t mv0 = esp_adc_cal_raw_to_voltage(raw0, &this->cal_characteristics_[(int32_t) ADC_ATTEN_DB_0]);

  // Contribution of each value, in range 0-2048 (12 bit ADC) or 0-4096 (13 bit ADC)
  uint32_t c12 = std::min(raw12, ADC_HALF);
  uint32_t c6 = ADC_HALF - std::abs(raw6 - ADC_HALF);
  uint32_t c2 = ADC_HALF - std::abs(raw2 - ADC_HALF);
  uint32_t c0 = std::min(ADC_MAX - raw0, ADC_HALF);
  // max theoretical csum value is 4096*4 = 16384
  uint32_t csum = c12 + c6 + c2 + c0;

  // each mv is max 3900; so max value is 3900*4096*4, fits in unsigned32
  uint32_t mv_scaled = (mv12 * c12) + (mv6 * c6) + (mv2 * c2) + (mv0 * c0);
  return mv_scaled / (float) (csum * 1000U);
}
#endif  // USE_ESP32

#ifdef USE_RP2040
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
  } else {
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
    float coeff = pin == PICO_VSYS_PIN ? 3.0 : 1.0;
    return raw * 3.3f / 4096.0f * coeff;
  }
}
#endif

#ifdef USE_LIBRETINY
float ADCSensor::sample() {
  uint32_t raw = 0;
  if (this->output_raw_) {
    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      raw += analogRead(this->pin_->get_pin());  // NOLINT
    }
    raw = (raw + (this->sample_count_ >> 1)) / this->sample_count_;  // NOLINT(clang-analyzer-core.DivideZero)
    return raw;
  }
  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
    raw += analogReadVoltage(this->pin_->get_pin());  // NOLINT
  }
  raw = (raw + (this->sample_count_ >> 1)) / this->sample_count_;  // NOLINT(clang-analyzer-core.DivideZero)
  return raw / 1000.0f;
}
#endif  // USE_LIBRETINY

#ifdef USE_ESP8266
std::string ADCSensor::unique_id() { return get_mac_address() + "-adc"; }
#endif

}  // namespace adc
}  // namespace esphome
