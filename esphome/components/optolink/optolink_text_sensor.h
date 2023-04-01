#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/text_sensor/text_sensor.h"
#include "optolink.h"
#include "optolink_sensor_base.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkTextSensor : public OptolinkSensorBase,
                           public esphome::text_sensor::TextSensor,
                           public esphome::PollingComponent {
 public:
  OptolinkTextSensor(Optolink *optolink) : OptolinkSensorBase(optolink) {}

  void set_raw(bool raw) { raw_ = raw; }

 protected:
  void setup() override;
  void update() override { optolink_->read_value(datapoint_); }

  const StringRef &get_sensor_name() override { return get_name(); }
  void value_changed(float state) override { publish_state(std::to_string(state)); };

 private:
  bool raw_ = false;
};

}  // namespace optolink
}  // namespace esphome

#endif
