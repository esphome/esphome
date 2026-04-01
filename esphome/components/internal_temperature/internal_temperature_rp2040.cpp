#ifdef USE_RP2040

#include "esphome/core/log.h"
#include "internal_temperature.h"

#include "Arduino.h"

namespace esphome::internal_temperature {

static const char *const TAG = "internal_temperature.rp2040";

void InternalTemperatureSensor::update() {
  float temperature = NAN;
  bool success = false;

  temperature = analogReadTemp();
  success = (temperature != 0.0f);

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

#endif  // USE_RP2040
