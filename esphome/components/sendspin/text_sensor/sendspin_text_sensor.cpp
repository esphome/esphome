#include "sendspin_text_sensor.h"

#if defined(USE_ESP32) && defined(USE_TEXT_SENSOR) && defined(USE_SENDSPIN_METADATA)

#include <esp_timer.h>

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

void SendspinTextSensor::schedule_publish_(const ServerMetadataStateObject &metadata, const std::string &value) {
  int64_t client_target = this->parent_->get_client_time(metadata.timestamp);
  if (client_target != 0) {
    int64_t delay_us = client_target - esp_timer_get_time();
    if (delay_us > 0) {
      uint32_t delay_ms = static_cast<uint32_t>(std::min(delay_us / 1000, (int64_t) 30000));
      this->set_timeout("metadata", delay_ms, [this, value]() { this->publish_if_changed_(value); });
      return;
    }
  }
  this->publish_if_changed_(value);
}

void SendspinTextSensor::setup() {
  switch (this->metadata_type_) {
    case SendspinMetadataTypes::TITLE: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.title.has_value()) {
          this->schedule_publish_(metadata, metadata.title.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::ARTIST: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.artist.has_value()) {
          this->schedule_publish_(metadata, metadata.artist.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::ALBUM: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.album.has_value()) {
          this->schedule_publish_(metadata, metadata.album.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::ALBUM_ARTIST: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.album_artist.has_value()) {
          this->schedule_publish_(metadata, metadata.album_artist.value());
        }
      });
      break;
    }
    case SendspinMetadataTypes::YEAR: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.year.has_value()) {
          char buf[7];
          snprintf(buf, sizeof(buf), "%d", metadata.year.value());
          this->schedule_publish_(metadata, buf);
        }
      });
      break;
    }
    case SendspinMetadataTypes::TRACK: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.track.has_value()) {
          char buf[7];
          snprintf(buf, sizeof(buf), "%d", metadata.track.value());
          this->schedule_publish_(metadata, buf);
        }
      });
      break;
    }
  }
}

}  // namespace sendspin
}  // namespace esphome

#endif
