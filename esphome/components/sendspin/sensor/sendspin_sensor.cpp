#include "sendspin_sensor.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_METADATA) && defined(USE_SENSOR)

#include <sendspin/metadata_role.h>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.sensor";

// --- SendspinTrackProgressSensor ---

void SendspinTrackProgressSensor::dump_config() { LOG_SENSOR("", "Sendspin Track Progress", this); }

// THREAD CONTEXT: Main loop. The registered metadata callback also fires on the main loop
// (SendspinHub dispatches metadata from client_->loop()).
void SendspinTrackProgressSensor::setup() {
  this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
    if (!metadata.progress.has_value()) {
      return;
    }
    const auto &progress = metadata.progress.value();
    if (progress.playback_speed == 0) {
      // Paused: freeze progress at the reported position and stop polling to save cycles.
      this->stop_poller();
      this->publish_state(progress.track_progress);
    } else {
      this->start_poller();
    }
  });
}

// THREAD CONTEXT: Main loop.
// Sendspin only pushes progress on state changes (play/pause/seek/speed change), not continuously during
// playback. The hub helper interpolates the current position from the last server update and the playback
// speed, giving us a fresh value on every poll.
void SendspinTrackProgressSensor::update() { this->publish_state(this->parent_->get_track_progress_ms()); }

// --- SendspinTrackDurationSensor ---

void SendspinTrackDurationSensor::dump_config() { LOG_SENSOR("", "Sendspin Track Duration", this); }

// THREAD CONTEXT: Main loop. The registered metadata callback also fires on the main loop
// (SendspinHub dispatches metadata from client_->loop()).
void SendspinTrackDurationSensor::setup() {
  this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
    if (metadata.progress.has_value()) {
      this->publish_if_changed_(metadata.progress.value().track_duration);
    }
  });
}

// Dedup to avoid frontend churn; Sensor::publish_state always notifies without checking for changes.
void SendspinTrackDurationSensor::publish_if_changed_(float value) {
  if (this->get_raw_state() != value) {
    this->publish_state(value);
  }
}

}  // namespace esphome::sendspin_

#endif
