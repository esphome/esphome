#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_METADATA) && defined(USE_SENSOR)

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/sensor/sensor.h"

#include <optional>

namespace esphome::sendspin_ {

class SendspinTrackProgressSensor : public sensor::Sensor, public SendspinPollingChild {
 public:
  void dump_config() override;
  void setup() override;
  void update() override;
};

enum class SendspinNumericMetadataTypes {
  TRACK_DURATION,
  YEAR,
  TRACK,
};

class SendspinMetadataSensor : public sensor::Sensor, public SendspinChild {
 public:
  void dump_config() override;
  void setup() override;

  void set_metadata_type(SendspinNumericMetadataTypes metadata_type) { this->metadata_type_ = metadata_type; }

 protected:
  std::optional<float> extract_value_(const sendspin::ServerMetadataStateObject &metadata) const;
  void publish_if_changed_(float value);

  SendspinNumericMetadataTypes metadata_type_;
};

}  // namespace esphome::sendspin_
#endif
