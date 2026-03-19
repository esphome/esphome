#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_TEXT_SENSOR) && defined(USE_SENDSPIN_METADATA)

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "esphome/core/component.h"

namespace esphome {
namespace sendspin {

enum class SendspinMetadataTypes {
  TITLE,
  ARTIST,
  ALBUM,
  ALBUM_ARTIST,
  YEAR,
  TRACK,
};

class SendspinTextSensor : public Component, public text_sensor::TextSensor, public Parented<SendspinHub> {
 public:
  void dump_config() override;
  void setup() override;

  void set_metadata_string_type(SendspinMetadataTypes metadata_type) { this->metadata_type_ = metadata_type; }

 protected:
  void publish_if_changed_(const std::string &value);
  void schedule_publish_(const ServerMetadataStateObject &metadata, const std::string &value);

  SendspinMetadataTypes metadata_type_;
};

}  // namespace sendspin
}  // namespace esphome
#endif
