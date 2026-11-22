#include "ads131m0x_sensor.h"

#ifdef USE_ESP32

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ads131m0x {

ESPHOME_LOG_TAG(TAG, "ads131m0x.sensor");

// Extra time on top of two windows before a snapshot is considered stale
static constexpr uint32_t STALE_MARGIN_MS = 1000;

static const LogString *sensor_type_to_str(ADS131M0xSensorType type) {
  switch (type) {
    case ADS131M0X_SENSOR_TYPE_RMS:
      return LOG_STR("rms");
    case ADS131M0X_SENSOR_TYPE_MEAN:
      return LOG_STR("mean");
    default:
      return LOG_STR("unknown");
  }
}

void ADS131M0xSensor::update() {
  if (!this->parent_->is_healthy()) {
    this->publish_state(NAN);
    return;
  }
  const Snapshot snapshot = this->parent_->get_snapshot();
  if (snapshot.sequence == 0 ||
      (millis() - snapshot.timestamp_ms) > 2 * this->parent_->get_window_ms() + STALE_MARGIN_MS) {
    this->publish_state(NAN);
    return;
  }
  this->publish_state(this->type_ == ADS131M0X_SENSOR_TYPE_MEAN ? snapshot.mean[this->channel_]
                                                                : snapshot.rms[this->channel_]);
}

void ADS131M0xSensor::dump_config() {
  LOG_SENSOR("", "ADS131M0x Sensor", this);
  ESP_LOGCONFIG(TAG,
                "  Channel: %u\n"
                "  Type: %s",
                this->channel_, LOG_STR_ARG(sensor_type_to_str(this->type_)));
}

}  // namespace esphome::ads131m0x

#endif  // USE_ESP32
