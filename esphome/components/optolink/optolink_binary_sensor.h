#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "optolink.h"
#include "optolink_sensor_base.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkBinarySensor : public OptolinkSensorBase,
                             public esphome::binary_sensor::BinarySensor,
                             public esphome::PollingComponent {
 public:
  OptolinkBinarySensor(Optolink *optolink) : OptolinkSensorBase(optolink) {
    bytes_ = 1;
    div_ratio_ = 1;
  }

 protected:
  void setup() override { setup_datapoint_(); }
  void update() override { optolink_->read_value(datapoint_); }

  const StringRef &get_sensor_name() override { return get_name(); }
  void value_changed(float state) override { publish_state(state); };
};
}  // namespace optolink
}  // namespace esphome

#endif
