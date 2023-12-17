#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/switch/switch.h"
#include "../datapoint_component.h"
#include "../optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkSwitch : public DatapointComponent, public esphome::switch_::Switch, public esphome::PollingComponent {
 public:
  OptolinkSwitch(Optolink *optolink) : DatapointComponent(optolink, true) {
    set_bytes(1);
    set_div_ratio(1);
  }

 protected:
  void setup() override { setup_datapoint(); }
  void update() override { datapoint_read_request(); }
  void write_state(bool value) override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(uint8_t state) override { publish_state(state); };
};

}  // namespace optolink
}  // namespace esphome

#endif
