#include "internal_temperature.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
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
#endif  // USE_ESP32
#ifdef USE_RP2040
#include "Arduino.h"
#endif  // USE_RP2040
#ifdef USE_BK72XX
extern "C" {
uint32_t temp_single_get_current_temperature(uint32_t *temp_value);
}
#endif  // USE_BK72XX
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#endif  // USE_ZEPHYR && USE_NRF52

namespace esphome {
namespace internal_temperature {

static const char *const TAG = "internal_temperature";
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
static const struct device *const DIE_TEMPERATURE_SENSOR = DEVICE_DT_GET_ONE(nordic_nrf_temp);
#endif  // USE_ZEPHYR && USE_NRF52
#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C5) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32C61) || defined(USE_ESP32_VARIANT_ESP32H2) || \
    defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
static temperature_sensor_handle_t tsensNew = NULL;
#endif  // USE_ESP32_VARIANT
#endif  // USE_ESP32

void InternalTemperatureSensor::update() {
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
  struct sensor_value value;
  int result = sensor_sample_fetch(DIE_TEMPERATURE_SENSOR);
  if (result != 0) {
    ESP_LOGE(TAG, "Failed to fetch nRF52 die temperature sample (%d)", result);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
    return;
  }

  result = sensor_channel_get(DIE_TEMPERATURE_SENSOR, SENSOR_CHAN_DIE_TEMP, &value);
  if (result != 0) {
    ESP_LOGE(TAG, "Failed to get nRF52 die temperature (%d)", result);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
    return;
  }

  const float temperature = value.val1 + (value.val2 / 1000000.0f);
  if (std::isfinite(temperature)) {
    this->publish_state(temperature);
  } else {
    ESP_LOGD(TAG, "Ignoring invalid nRF52 temperature (value=%.1f)", temperature);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
  }
#else

  float temperature = NAN;
  bool success = false;
#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32)
  uint8_t raw = temprature_sens_read();
  ESP_LOGV(TAG, "Raw temperature value: %d", raw);
  temperature = (raw - 32) / 1.8f;
  success = (raw != 128);
#elif defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C5) || defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32C61) || \
    defined(USE_ESP32_VARIANT_ESP32H2) || defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || \
    defined(USE_ESP32_VARIANT_ESP32S3)
  esp_err_t result = temperature_sensor_get_celsius(tsensNew, &temperature);
  success = (result == ESP_OK);
  if (!success) {
    ESP_LOGE(TAG, "Reading failed (%d)", result);
  }
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
#endif  // USE_ZEPHYR && USE_NRF52
}

void InternalTemperatureSensor::setup() {
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
  if (!device_is_ready(DIE_TEMPERATURE_SENSOR)) {
    ESP_LOGE(TAG, "nRF52 die temperature sensor device %s not ready", DIE_TEMPERATURE_SENSOR->name);
    this->mark_failed();
    return;
  }
#endif  // USE_ZEPHYR && USE_NRF52
#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C5) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32C61) || defined(USE_ESP32_VARIANT_ESP32H2) || \
    defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  temperature_sensor_config_t tsens_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

  esp_err_t result = temperature_sensor_install(&tsens_config, &tsensNew);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Install failed (%d)", result);
    this->mark_failed();
    return;
  }

  result = temperature_sensor_enable(tsensNew);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Enabling failed (%d)", result);
    this->mark_failed();
    return;
  }
#endif  // USE_ESP32_VARIANT
#endif  // USE_ESP32
}

void InternalTemperatureSensor::dump_config() { LOG_SENSOR("", "Internal Temperature Sensor", this); }

}  // namespace internal_temperature
}  // namespace esphome
