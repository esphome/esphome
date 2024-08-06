#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../optolink.h"
#include "../datapoint_component.h"

namespace esphome {
namespace optolink {

class OptolinkBinarySensor : public DatapointComponent, public esphome::binary_sensor::BinarySensor {
 public:
  OptolinkBinarySensor(Optolink *optolink) : DatapointComponent(optolink) {
    set_bytes(1);
    set_div_ratio(1);
  }

 protected:
  void setup() override { setup_datapoint_(); }
  void update() override { datapoint_read_request_(); }

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(uint8_t value) override { publish_state(value); };
};
}  // namespace optolink
}  // namespace esphome

#endif
