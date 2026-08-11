#include "sendspin_text_sensor.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_METADATA) && defined(USE_TEXT_SENSOR)

#include <sendspin/metadata_role.h>

#include <string>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.text_sensor";

void SendspinTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Sendspin", this); }

// A field is nullopt when the server has not provided it or has explicitly cleared it (e.g. a track with no album
// name). Both mean the same thing to the frontend; i.e., there is nothing to show, so the empty string is returned for
// either, and the caller publishes it. Returning early instead would leave the previous track's value on display.
const char *SendspinTextSensor::extract_value_(const sendspin::ServerMetadataStateObject &metadata) const {
  switch (this->metadata_type_) {
    case SendspinTextMetadataTypes::TITLE:
      return metadata.title.has_value() ? metadata.title.value().c_str() : "";
    case SendspinTextMetadataTypes::ARTIST:
      return metadata.artist.has_value() ? metadata.artist.value().c_str() : "";
    case SendspinTextMetadataTypes::ALBUM:
      return metadata.album.has_value() ? metadata.album.value().c_str() : "";
    case SendspinTextMetadataTypes::ALBUM_ARTIST:
      return metadata.album_artist.has_value() ? metadata.album_artist.value().c_str() : "";
  }
  return "";
}

// THREAD CONTEXT: Main loop. The registered metadata callback also fires on the main loop
// (SendspinHub dispatches metadata from client_->loop()).
void SendspinTextSensor::setup() {
  this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
    this->publish_if_changed_(this->extract_value_(metadata));
  });
}

// Dedup to avoid frontend churn; TextSensor::publish_state already dedups the string assign but still notifies.
void SendspinTextSensor::publish_if_changed_(const char *value) {
  if (!this->has_state()) {
    // Nothing published yet, so the frontend already shows this as unknown: only a real value is news. Publishing the
    // empty string here would just fire on_value with nothing in it on the first metadata of every connection.
    if (*value != '\0') {
      this->publish_state(value);
    }
    return;
  }
  if (this->get_raw_state() != value) {
    this->publish_state(value);
  }
}

}  // namespace esphome::sendspin_

#endif
