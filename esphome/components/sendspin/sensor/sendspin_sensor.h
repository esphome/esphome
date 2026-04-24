#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_METADATA) && defined(USE_SENSOR)

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::sendspin_ {

class SendspinTrackProgressSensor : public sensor::Sensor, public SendspinPollingChild {
 public:
  void dump_config() override;
  void setup() override;
  void update() override;
};

class SendspinTrackDurationSensor : public sensor::Sensor, public SendspinChild {
 public:
  void dump_config() override;
  void setup() override;

 protected:
  void publish_if_changed_(float value);
};

}  // namespace esphome::sendspin_
#endif
