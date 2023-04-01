#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/switch/switch.h"
#include "optolink_sensor_base.h"
#include "optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkSwitch : public OptolinkSensorBase, public esphome::switch_::Switch, public esphome::PollingComponent {
 public:
  OptolinkSwitch(Optolink *optolink) : OptolinkSensorBase(optolink, true) {
    bytes_ = 1;
    div_ratio_ = 1;
  }

 protected:
  void setup() override { setup_datapoint_(); }
  void update() override { optolink_->read_value(datapoint_); }

  const StringRef &get_sensor_name() override { return get_name(); }
  void value_changed(float state) override { publish_state(state); };

  void write_state(bool value) override;
};

}  // namespace optolink
}  // namespace esphome

#endif
