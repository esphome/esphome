#include "internal_temperature.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32)
// there is no official API available on the original ESP32
extern "C" {
uint8_t temprature_sens_read();
}
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "driver/temp_sensor.h"
#endif  // USE_ESP32_VARIANT
#endif  // USE_ESP32
#ifdef USE_RP2040
#include "Arduino.h"
#endif  // USE_RP2040
#ifdef USE_BK72XX
extern "C" {
uint32_t temp_single_get_current_temperature(uint32_t *temp_value);
}
#endif  // USE_BK72XX

namespace esphome {
namespace internal_temperature {

static const char *const TAG = "internal_temperature";

void InternalTemperatureSensor::update() {
  float temperature = NAN;
  bool success = false;
#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32)
  uint8_t raw = temprature_sens_read();
  ESP_LOGV(TAG, "Raw temperature value: %d", raw);
  temperature = (raw - 32) / 1.8f;
  success = (raw != 128);
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  temp_sensor_config_t tsens = TSENS_CONFIG_DEFAULT();
  temp_sensor_set_config(tsens);
  temp_sensor_start();
#if defined(USE_ESP32_VARIANT_ESP32S3) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 3))
#error \
    "ESP32-S3 internal temperature sensor requires ESP IDF V4.4.3 or higher. See https://github.com/esphome/issues/issues/4271"
#endif
  esp_err_t result = temp_sensor_read_celsius(&temperature);
  temp_sensor_stop();
  success = (result == ESP_OK);
#endif  // USE_ESP32_VARIANT
#endif  // USE_ESP32
#ifdef USE_RP2040
  temperature = analogReadTemp();
  success = (temperature != 0.0f);
#endif  // USE_RP2040
#ifdef USE_BK72XX
  uint32_t raw, result;
  result = temp_single_get_current_temperature(&raw);
  success = (result == 0);
#if defined(USE_LIBRETINY_VARIANT_BK7231N)
  temperature = raw * -0.38f + 156.0f;
#elif defined(USE_LIBRETINY_VARIANT_BK7231T)
  temperature = raw * 0.04f;
#else   // USE_LIBRETINY_VARIANT
  temperature = raw * 0.128f;
#endif  // USE_LIBRETINY_VARIANT
#endif  // USE_BK72XX
  if (success && std::isfinite(temperature)) {
    this->publish_state(temperature);
  } else {
    ESP_LOGD(TAG, "Ignoring invalid temperature (success=%d, value=%.1f)", success, temperature);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
  }
}

void InternalTemperatureSensor::dump_config() { LOG_SENSOR("", "Internal Temperature Sensor", this); }

}  // namespace internal_temperature
}  // namespace esphome
