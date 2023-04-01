#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/number/number.h"
#include "optolink_sensor_base.h"
#include "optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkNumber : public OptolinkSensorBase, public esphome::number::Number, public esphome::PollingComponent {
 public:
  OptolinkNumber(Optolink *optolink) : OptolinkSensorBase(optolink, true) {}

 protected:
  void setup() override { setup_datapoint_(); }
  void update() override { optolink_->read_value(datapoint_); }

  const StringRef &get_sensor_name() override { return get_name(); }
  void value_changed(float state) override { publish_state(state); };

  void control(float value) override;
};

}  // namespace optolink
}  // namespace esphome

#endif
