#include "ads131m0x_sensor.h"

#ifdef USE_ESP32

#include <cmath>

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::ads131m0x {

ESPHOME_LOG_TAG(TAG, "ads131m0x.sensor");

// Extra time on top of two windows before a snapshot is considered stale
static constexpr uint32_t STALE_MARGIN_MS = 1000;

// Indexed by ADS131M0xSensorType; the last entry is the fallback for out-of-range values
PROGMEM_STRING_TABLE(SensorTypeStrings, "rms", "mean", "unknown");

void ADS131M0xSensor::update() {
  if (!this->parent_->is_healthy()) {
    this->publish_state(NAN);
    return;
  }
  const Snapshot snapshot = this->parent_->get_snapshot();
  // The sampling task stamps windows with millis(), which can be later than the cached loop start time:
  // a negative age is a fresh window, not an overflow.
  const int32_t age_ms = static_cast<int32_t>(App.get_loop_component_start_time() - snapshot.timestamp_ms);
  if (snapshot.sequence == 0 || age_ms > static_cast<int32_t>(2 * this->parent_->get_window_ms() + STALE_MARGIN_MS)) {
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
                this->channel_, LOG_STR_ARG(SensorTypeStrings::get_log_str(this->type_)));
}

}  // namespace esphome::ads131m0x

#endif  // USE_ESP32
