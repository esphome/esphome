#ifdef USE_BK72XX

#include "esphome/core/log.h"
#include "internal_temperature.h"

extern "C" {
uint32_t temp_single_get_current_temperature(uint32_t *temp_value);
}

namespace esphome::internal_temperature {

static const char *const TAG = "internal_temperature.bk72xx";

void InternalTemperatureSensor::update() {
  float temperature = NAN;
  bool success = false;

  uint32_t raw, result;
  result = temp_single_get_current_temperature(&raw);
  success = (result == 0);
#if defined(USE_LIBRETINY_VARIANT_BK7231N)
  temperature = raw * -0.38f + 156.0f;
#else   // USE_LIBRETINY_VARIANT
  temperature = raw * 0.128f;
#endif  // USE_LIBRETINY_VARIANT

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

#endif  // USE_BK72XX
