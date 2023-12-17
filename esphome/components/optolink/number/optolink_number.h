#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/number/number.h"
#include "../datapoint_component.h"
#include "../optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkNumber : public DatapointComponent, public esphome::number::Number, public esphome::PollingComponent {
 public:
  OptolinkNumber(Optolink *optolink) : DatapointComponent(optolink, true) {}

 protected:
  void setup() override { setup_datapoint(); }
  void update() override { datapoint_read_request(); }
  void control(float value) override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(float state) override { publish_state(state); };
  void datapoint_value_changed(uint8_t state) override { publish_state(state); };
  void datapoint_value_changed(uint16_t state) override { publish_state(state); };
  void datapoint_value_changed(uint32_t state) override { publish_state(state); };
};

}  // namespace optolink
}  // namespace esphome

#endif
