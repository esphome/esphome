#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/sensor/sensor.h"
#include "../optolink.h"
#include "../datapoint_component.h"
#include <cfloat>

namespace esphome {
namespace optolink {

class OptolinkSensor : public DatapointComponent, public esphome::sensor::Sensor, public esphome::PollingComponent {
 public:
  OptolinkSensor(Optolink *optolink) : DatapointComponent(optolink) {
    set_state_class(esphome::sensor::STATE_CLASS_MEASUREMENT);
  }

  void set_min_value(float min_value);

 protected:
  void setup() { setup_datapoint(); }
  void update() override { datapoint_read_request(); }

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(float state) override { publish_state(state); };
  void datapoint_value_changed(uint8_t state) override;
  void datapoint_value_changed(uint16_t state) override;
  void datapoint_value_changed(uint32_t state) override;

 private:
  float min_value_ = -FLT_MAX;
};
}  // namespace optolink
}  // namespace esphome

#endif
