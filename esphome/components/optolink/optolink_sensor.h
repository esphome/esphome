#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/sensor/sensor.h"
#include "optolink.h"
#include "optolink_sensor_base.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkSensor : public OptolinkSensorBase, public esphome::sensor::Sensor, public esphome::PollingComponent {
 public:
  OptolinkSensor(Optolink *optolink) : OptolinkSensorBase(optolink) {
    set_state_class(esphome::sensor::STATE_CLASS_MEASUREMENT);
  }

 protected:
  void setup() { setup_datapoint_(); }
  void update() override { optolink_->read_value(datapoint_); }

  const StringRef &get_sensor_name() override { return get_name(); }
  void value_changed(float state) override { publish_state(state); };
};
}  // namespace optolink
}  // namespace esphome

#endif
