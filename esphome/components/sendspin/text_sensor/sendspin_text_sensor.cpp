#include "sendspin_text_sensor.h"

#if defined(USE_ESP_IDF) && defined(USE_TEXT_SENSOR) && defined(USE_SENDSPIN_METADATA)

#include <string>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.text_sensor";

void SendspinTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Sendspin", this); }

void SendspinTextSensor::publish_if_changed_(const std::string &value) {
  if (this->get_raw_state() != value) {
    this->publish_state(value);
  }
}

void SendspinTextSensor::setup() {
  switch (this->metadata_type_) {
    case SendspinMetadataTypes::TITLE: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.title.has_value()) {
          this->publish_if_changed_(metadata.title.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::ARTIST: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.artist.has_value()) {
          this->publish_if_changed_(metadata.artist.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::ALBUM: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.album.has_value()) {
          this->publish_if_changed_(metadata.album.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::ALBUM_ARTIST: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.album_artist.has_value()) {
          this->publish_if_changed_(metadata.album_artist.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::YEAR: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.year.has_value()) {
          this->publish_if_changed_(std::to_string(metadata.year.value()));
        }
      });
      break;
    }
    case SendspinMetadataTypes::TRACK: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.track.has_value()) {
          this->publish_if_changed_(std::to_string(metadata.track.value()));
        }
      });
      break;
    }
  }
}

}  // namespace sendspin
}  // namespace esphome

#endif
