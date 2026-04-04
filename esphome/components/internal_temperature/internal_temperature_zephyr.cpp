#if defined(USE_ZEPHYR) && defined(USE_NRF52)

#include "esphome/core/log.h"
#include "internal_temperature.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

namespace esphome::internal_temperature {

static const char *const TAG = "internal_temperature.zephyr";

static const struct device *const DIE_TEMPERATURE_SENSOR = DEVICE_DT_GET_ONE(nordic_nrf_temp);

void InternalTemperatureSensor::update() {
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
}

void InternalTemperatureSensor::setup() {
  if (!device_is_ready(DIE_TEMPERATURE_SENSOR)) {
    ESP_LOGE(TAG, "nRF52 die temperature sensor device %s not ready", DIE_TEMPERATURE_SENSOR->name);
    this->mark_failed();
    return;
  }
}

}  // namespace esphome::internal_temperature

#endif  // USE_ZEPHYR && USE_NRF52
