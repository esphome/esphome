#include "internal_temperature.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_VARIANT_ESP32
// there is no official API available on the original ESP32
extern "C" {
uint8_t temprature_sens_read();
}
#else  // USE_ESP32_VARIANT_ESP32
#include "driver/temp_sensor.h"
#endif  // USE_ESP32_VARIANT_ESP32
#endif  // USE_ESP32
#ifdef USE_RP2040
#include "Arduino.h"
#endif  // USE_RP2040

namespace esphome {
namespace internal_temperature {

static const char *const TAG = "internal_temperature";

void InternalTemperatureSensor::update() {
  float temperature = NAN;
  bool success = false;
#ifdef USE_ESP32
#ifdef CONFIG_IDF_TARGET_ESP32
  uint8_t raw = temprature_sens_read();
  ESP_LOGV(TAG, "Raw temperature value: %d", raw);
  temperature = (raw - 32) / 1.8f;
  success = (raw != 128);
#else   // CONFIG_IDF_TARGET_ESP32
  temp_sensor_config_t tsens = TSENS_CONFIG_DEFAULT();
  temp_sensor_set_config(tsens);
  temp_sensor_start();
  esp_err_t result = temp_sensor_read_celsius(&temperature);
  temp_sensor_stop();
  success = (result == ESP_OK);
#endif  // CONFIG_IDF_TARGET_ESP32
#endif  // USE_ESP32
#if USE_RP2040
  temperature = analogReadTemp();
  success = (temperature != 0.0f);
#endif  // USE_RP2040
  if (success && std::isfinite(temperature)) {
    ESP_LOGD(TAG, "Got temperature: %.1f°C", temperature);
    this->publish_state(temperature);
  } else {
    ESP_LOGD(TAG, "Ignoring invalid temperature (success=%d, value=%.1f)", success, temperature);
    if (!this->has_state()) {
      this->publish_state(NAN);
    }
  }
}

std::string InternalTemperatureSensor::unique_id() { return get_mac_address() + "-internaltemp"; }
void InternalTemperatureSensor::dump_config() { LOG_SENSOR("", "Internal Temperature Sensor", this); }

}  // namespace internal_temperature
}  // namespace esphome
