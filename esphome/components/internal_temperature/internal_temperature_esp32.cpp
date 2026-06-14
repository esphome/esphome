#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "internal_temperature.h"

#if defined(USE_ESP32_VARIANT_ESP32)
// there is no official API available on the original ESP32
extern "C" {
uint8_t temprature_sens_read();
}
#elif defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C5) || defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32C61) || \
    defined(USE_ESP32_VARIANT_ESP32H2) || defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || \
    defined(USE_ESP32_VARIANT_ESP32S3)
#include "driver/temperature_sensor.h"
#endif  // USE_ESP32_VARIANT

namespace esphome::internal_temperature {

static const char *const TAG = "internal_temperature.esp32";

void InternalTemperatureSensor::update() {
  float temperature = NAN;
  bool success = false;
#if defined(USE_ESP32_VARIANT_ESP32)
  uint8_t raw = temprature_sens_read();
  ESP_LOGV(TAG, "Raw temperature value: %d", raw);
  temperature = (raw - 32) / 1.8f;
  success = (raw != 128);
#elif defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C5) || defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32C61) || \
    defined(USE_ESP32_VARIANT_ESP32H2) || defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || \
    defined(USE_ESP32_VARIANT_ESP32S3)
  esp_err_t result = temperature_sensor_get_celsius(this->tsens_, &temperature);
  success = (result == ESP_OK);
  if (!success) {
    ESP_LOGE(TAG, "Reading failed (%d)", result);
  }
#endif  // USE_ESP32_VARIANT

  if (success && std::isfinite(temperature)) {
    this->publish_state(temperature);
  } else {
    ESP_LOGD(TAG, "Ignoring invalid temperature (success=%d, value=%.1f)", success, temperature);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
  }
}

void InternalTemperatureSensor::setup() {
#if defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C5) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32C61) || defined(USE_ESP32_VARIANT_ESP32H2) || \
    defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  temperature_sensor_config_t tsens_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

  esp_err_t result = temperature_sensor_install(&tsens_config, &this->tsens_);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Install failed (%d)", result);
    this->mark_failed();
    return;
  }

  result = temperature_sensor_enable(this->tsens_);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Enabling failed (%d)", result);
    this->mark_failed();
    return;
  }
#endif  // USE_ESP32_VARIANT
}

}  // namespace esphome::internal_temperature

#endif  // USE_ESP32
