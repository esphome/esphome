#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/switch/switch.h"
#include "../optolink.h"
#include "../datapoint_component.h"

namespace esphome {
namespace optolink {

class OptolinkSwitch : public DatapointComponent, public esphome::switch_::Switch {
 public:
  OptolinkSwitch(Optolink *optolink) : DatapointComponent(optolink, true) {
    set_bytes(1);
    set_div_ratio(1);
  }

 protected:
  void setup() override { setup_datapoint_(); }
  void update() override { datapoint_read_request_(); }
  void write_state(bool value) override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(uint8_t value) override { publish_state((bool) value); };
};

}  // namespace optolink
}  // namespace esphome

#endif
