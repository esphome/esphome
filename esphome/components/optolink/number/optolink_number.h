#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/number/number.h"
#include "../optolink.h"
#include "../datapoint_component.h"

namespace esphome {
namespace optolink {

class OptolinkNumber : public DatapointComponent, public esphome::number::Number, public esphome::PollingComponent {
 public:
  OptolinkNumber(Optolink *optolink) : DatapointComponent(optolink, true) {}

 protected:
  void setup() override { setup_datapoint_(); }
  void update() override { datapoint_read_request_(); }
  void control(float value) override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(float value) override { publish_state(value); };
  void datapoint_value_changed(uint8_t value) override;
  void datapoint_value_changed(uint16_t value) override;
  void datapoint_value_changed(uint32_t value) override;
};

}  // namespace optolink
}  // namespace esphome

#endif
