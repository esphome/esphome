#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_TEXT_SENSOR) && defined(USE_RESONATE_METADATA)

#include "esphome/components/resonate/resonate_hub.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "esphome/core/component.h"

namespace esphome {
namespace resonate {

enum class ResonateMetadataTypes {
  TITLE,
  ARTIST,
  ALBUM,
  YEAR,
  TRACK,
};

class ResonateTextSensor : public Component, public text_sensor::TextSensor, public Parented<ResonateHub> {
 public:
  void dump_config() override;
  void setup() override;

  void set_metadata_string_type(ResonateMetadataTypes metadata_type) { this->metadata_type_ = metadata_type; }

 protected:
  ResonateMetadataTypes metadata_type_;
};

}  // namespace resonate
}  // namespace esphome
#endif
