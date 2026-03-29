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
#include "hal/nrf_temp.h"
#endif  // USE_ZEPHYR && USE_NRF52

namespace esphome {
namespace internal_temperature {

static const char *const TAG = "internal_temperature";
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
static constexpr uint32_t NRF52_TEMP_READY_TIMEOUT_ID = 1;
static constexpr uint32_t NRF52_TEMP_POLL_DELAY_MS = 1;
static constexpr uint8_t NRF52_TEMP_MAX_POLLS = 5;
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
  this->cancel_timeout(NRF52_TEMP_READY_TIMEOUT_ID);
  nrf_temp_event_clear(NRF_TEMP, NRF_TEMP_EVENT_DATARDY);
  nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_START);
  this->set_timeout(NRF52_TEMP_READY_TIMEOUT_ID, NRF52_TEMP_POLL_DELAY_MS,
                    [this]() { this->poll_nrf52_temperature_(NRF52_TEMP_MAX_POLLS); });
  return;
#endif  // USE_ZEPHYR && USE_NRF52

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
}

#if defined(USE_ZEPHYR) && defined(USE_NRF52)
void InternalTemperatureSensor::poll_nrf52_temperature_(uint8_t attempts_left) {
  if (!nrf_temp_event_check(NRF_TEMP, NRF_TEMP_EVENT_DATARDY)) {
    if (attempts_left > 0) {
      this->set_timeout(NRF52_TEMP_READY_TIMEOUT_ID, NRF52_TEMP_POLL_DELAY_MS,
                        [this, attempts_left]() { this->poll_nrf52_temperature_(attempts_left - 1); });
      return;
    }

    nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_STOP);
    ESP_LOGE(TAG, "Timed out reading nRF52 internal temperature");
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
    return;
  }

  nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_STOP);

  const int32_t raw_temperature = nrf_temp_result_get(NRF_TEMP);
  const float temperature = raw_temperature / 4.0f;
  if (std::isfinite(temperature)) {
    this->publish_state(temperature);
  } else {
    ESP_LOGD(TAG, "Ignoring invalid temperature (value=%.1f)", temperature);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
  }
}
#endif  // USE_ZEPHYR && USE_NRF52

void InternalTemperatureSensor::setup() {
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
