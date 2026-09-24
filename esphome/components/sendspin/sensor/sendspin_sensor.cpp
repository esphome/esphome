#include "sendspin_sensor.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_METADATA) && defined(USE_SENSOR)

#include <sendspin/metadata_role.h>

#include <cmath>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.sensor";

// --- SendspinTrackProgressSensor ---

void SendspinTrackProgressSensor::dump_config() {
  LOG_SENSOR("", "Track Progress", this);
  LOG_UPDATE_INTERVAL(this);
}

// THREAD CONTEXT: Main loop. The registered metadata callback also fires on the main loop
// (SendspinHub dispatches metadata from client_->loop()).
void SendspinTrackProgressSensor::setup() {
  this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
    if (!metadata.progress.has_value()) {
      // Progress is unknown: the server has not reported it, or it was cleared (e.g. on disconnect). Stop polling and
      // report unknown rather than leaving the last position frozen on the frontend. Only the transition is published;
      // NAN never compares equal to itself, so an unguarded publish would repeat on every metadata update.
      this->stop_poller();
      if (!std::isnan(this->get_raw_state())) {
        this->publish_state(NAN);
      }
      return;
    }
    const auto &progress = metadata.progress.value();
    if (progress.playback_speed == 0) {
      // Paused: freeze progress at the reported position and stop polling to save cycles.
      this->stop_poller();
      this->publish_state(progress.track_progress);
    } else {
      // Resumed: publish the fresh interpolated position immediately so the frontend doesn't show a stale
      // paused value until the next poll tick.
      this->publish_state(this->parent_->get_track_progress_ms());
      this->start_poller();
    }
  });

  // PollingComponent starts the poller before setup(), but there is nothing to interpolate yet:
  // get_track_progress_ms() returns 0 until the server reports a position, so polling now would publish 0 every tick
  // from boot until the first metadata arrives. The callback above starts it once playback is running.
  this->stop_poller();
}

// THREAD CONTEXT: Main loop.
// Sendspin only pushes progress on state changes (play/pause/seek/speed change), not continuously during
// playback. The hub helper interpolates the current position from the last server update and the playback
// speed, giving us a fresh value on every poll.
void SendspinTrackProgressSensor::update() { this->publish_state(this->parent_->get_track_progress_ms()); }

// --- SendspinMetadataSensor ---

void SendspinMetadataSensor::dump_config() {
  switch (this->metadata_type_) {
    case SendspinNumericMetadataTypes::TRACK_DURATION:
      LOG_SENSOR("", "Track Duration", this);
      break;
    case SendspinNumericMetadataTypes::YEAR:
      LOG_SENSOR("", "Year", this);
      break;
    case SendspinNumericMetadataTypes::TRACK:
      LOG_SENSOR("", "Track", this);
      break;
  }
}

std::optional<float> SendspinMetadataSensor::extract_value_(const sendspin::ServerMetadataStateObject &metadata) const {
  switch (this->metadata_type_) {
    case SendspinNumericMetadataTypes::TRACK_DURATION:
      if (metadata.progress.has_value())
        return metadata.progress.value().track_duration;
      return std::nullopt;
    case SendspinNumericMetadataTypes::YEAR:
      if (metadata.year.has_value())
        return metadata.year.value();
      return std::nullopt;
    case SendspinNumericMetadataTypes::TRACK:
      if (metadata.track.has_value())
        return metadata.track.value();
      return std::nullopt;
  }
  return std::nullopt;
}

// THREAD CONTEXT: Main loop. The registered metadata callback also fires on the main loop
// (SendspinHub dispatches metadata from client_->loop()).
void SendspinMetadataSensor::setup() {
  this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
    // A field the server has not provided, or has explicitly cleared, is published as NAN (the sensor convention for
    // unknown) rather than skipped, so a value that goes away does not linger from the previous track.
    this->publish_if_changed_(this->extract_value_(metadata).value_or(NAN));
  });
}

// Dedup to avoid frontend churn; Sensor::publish_state always notifies without checking for changes.
void SendspinMetadataSensor::publish_if_changed_(float value) {
  const float current = this->get_raw_state();
  // The raw state starts as NAN, so a field that is already cleared when the first update arrives is suppressed here
  // as well: the frontend still shows the sensor as unknown, which is what a clear means. NAN never compares equal to
  // itself, so a field that stays cleared would republish on every metadata update without the second check.
  if (current != value && !(std::isnan(current) && std::isnan(value))) {
    this->publish_state(value);
  }
}

}  // namespace esphome::sendspin_

#endif
