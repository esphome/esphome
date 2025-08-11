#include "resonate_text_sensor.h"

#if defined(USE_ESP_IDF) && defined(USE_TEXT_SENSOR) && defined(USE_RESONATE_METADATA)

#include <string>

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.text_sensor";

void ResonateTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Resonate", this); }

void ResonateTextSensor::setup() {
  switch (this->metadata_type_) {
    case ResonateMetadataTypes::TITLE: {
      this->parent_->add_metadata_callback(
          [this](const ResonateMetadata &metadata) { this->publish_state(metadata.title); });
      break;
    }
    case ResonateMetadataTypes::ARTIST: {
      this->parent_->add_metadata_callback(
          [this](const ResonateMetadata &metadata) { this->publish_state(metadata.artist); });
      break;
    }
    case ResonateMetadataTypes::ALBUM: {
      this->parent_->add_metadata_callback(
          [this](const ResonateMetadata &metadata) { this->publish_state(metadata.album); });
      break;
    }
    case ResonateMetadataTypes::YEAR: {
      this->parent_->add_metadata_callback(
          [this](const ResonateMetadata &metadata) { this->publish_state(std::to_string(metadata.year)); });
      break;
    }
    case ResonateMetadataTypes::TRACK: {
      this->parent_->add_metadata_callback(
          [this](const ResonateMetadata &metadata) { this->publish_state(std::to_string(metadata.track)); });
      break;
    }
  }
}

}  // namespace resonate
}  // namespace esphome

#endif
