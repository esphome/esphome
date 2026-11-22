#include "sendspin_text_sensor.h"

#if defined(USE_ESP_IDF) && defined(USE_TEXT_SENSOR)

#ifdef USE_SENDSPIN_METADATA
#include <sendspin/metadata_role.h>
#endif

#include <string>

namespace esphome::sendspin_ {

ESPHOME_LOG_TAG(TAG, "sendspin.text_sensor");

#ifdef USE_SENDSPIN_METADATA
void SendspinTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Sendspin", this); }

// A field is nullopt when the server has not provided it or has explicitly cleared it. Both mean there is nothing to
// show, so return the empty string and let the caller publish it; returning early would leave the previous track's
// value on display.
//
// The empty string is not the same as unknown. A text sensor reports unknown through the API's missing_state flag,
// which follows has_state(), and has_state() is only ever set, never cleared. Once a real value has been published,
// an empty state is the closest we can get. The numeric sensors publish NAN, which does read as unknown.
StringRef SendspinTextSensor::extract_value_(const sendspin::TrackMetadata &metadata) const {
  const auto text = [](const auto &field) {
    return field.has_value() ? StringRef(field->data(), field->size()) : StringRef();
  };
  switch (this->metadata_type_) {
    case SendspinTextMetadataTypes::TITLE:
      return text(metadata.title);
    case SendspinTextMetadataTypes::ARTIST:
      return text(metadata.artist);
    case SendspinTextMetadataTypes::ALBUM:
      return text(metadata.album);
    case SendspinTextMetadataTypes::ALBUM_ARTIST:
      return text(metadata.album_artist);
  }
  return StringRef();
}

// THREAD CONTEXT: Main loop. The registered metadata callback also fires on the main loop
// (SendspinHub dispatches metadata from client_->loop()).
void SendspinTextSensor::setup() {
  this->parent_->add_metadata_update_callback(
      [this](const sendspin::TrackMetadata &metadata) { this->publish_if_changed_(this->extract_value_(metadata)); });
}

// Dedup to avoid frontend churn; TextSensor::publish_state already dedups the string assign but still notifies.
void SendspinTextSensor::publish_if_changed_(StringRef value) {
  // The state starts empty, so a field that is already cleared when the first update arrives is suppressed here: the
  // entity stays unknown rather than being dropped out of it for good by an empty publish. Later clears do publish the
  // empty string and fire on_value with it.
  if (this->get_raw_state() != value) {
    this->publish_state(value.c_str(), value.size());
  }
}
#endif  // USE_SENDSPIN_METADATA

void SendspinPairingCodeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Pairing Code", this); }

// THREAD CONTEXT: Main loop. The registered callbacks also fire on the main loop (SendspinHub dispatches them from
// its loop()).
void SendspinPairingCodeTextSensor::setup() {
  this->parent_->add_on_display_pairing_code_callback([this](const std::string &code) { this->publish_state(code); });
  this->parent_->add_on_clear_pairing_code_callback([this]() { this->publish_state(""); });
}

}  // namespace esphome::sendspin_

#endif
