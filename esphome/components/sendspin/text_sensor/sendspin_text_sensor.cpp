#include "sendspin_text_sensor.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_METADATA)

#include <sendspin/metadata_role.h>

#include <cstdio>
#include <string>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.text_sensor";

void SendspinTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Sendspin", this); }

// THREAD CONTEXT: Main loop. The registered metadata callback also fires on the main loop
// (SendspinHub dispatches metadata from client_->loop()).
void SendspinTextSensor::setup() {
  switch (this->metadata_type_) {
    case SendspinTextMetadataTypes::TITLE: {
      this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
        if (metadata.title.has_value()) {
          this->publish_if_changed_(metadata.title.value());
        }
      });
      break;
    }
    case SendspinTextMetadataTypes::ARTIST: {
      this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
        if (metadata.artist.has_value()) {
          this->publish_if_changed_(metadata.artist.value());
        }
      });
      break;
    }
    case SendspinTextMetadataTypes::ALBUM: {
      this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
        if (metadata.album.has_value()) {
          this->publish_if_changed_(metadata.album.value());
        }
      });
      break;
    }
    case SendspinTextMetadataTypes::ALBUM_ARTIST: {
      this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
        if (metadata.album_artist.has_value()) {
          this->publish_if_changed_(metadata.album_artist.value());
        }
      });
      break;
    }
    case SendspinTextMetadataTypes::YEAR: {
      this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
        if (metadata.year.has_value()) {
          char buf[7];
          snprintf(buf, sizeof(buf), "%d", metadata.year.value());
          this->publish_if_changed_(buf);
        }
      });
      break;
    }
    case SendspinTextMetadataTypes::TRACK: {
      this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
        if (metadata.track.has_value()) {
          char buf[7];
          snprintf(buf, sizeof(buf), "%d", metadata.track.value());
          this->publish_if_changed_(buf);
        }
      });
      break;
    }
  }
}

void SendspinTextSensor::publish_if_changed_(const std::string &value) {
  if (this->get_raw_state() != value) {
    this->publish_state(value);
  }
}

}  // namespace esphome::sendspin_

#endif
