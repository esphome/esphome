#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/text_sensor/text_sensor.h"
#include "optolink.h"
#include "optolink_sensor_base.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

enum TextSensorMode { MAP, RAW, DAY_SCHEDULE };

class OptolinkTextSensor : public OptolinkSensorBase,
                           public esphome::text_sensor::TextSensor,
                           public esphome::PollingComponent {
 public:
  OptolinkTextSensor(Optolink *optolink) : OptolinkSensorBase(optolink) {}

  void set_mode(TextSensorMode mode) { mode_ = mode; }
  void set_day_of_week(int dow) { dow_ = dow; }

 protected:
  void setup() override;
  void update() override { optolink_->read_value(datapoint_); }

  const StringRef &get_sensor_name() override { return get_name(); }
  void value_changed(float state) override { publish_state(std::to_string((uint32_t) state)); };

 private:
  TextSensorMode mode_ = MAP;
  int dow_ = 0;
};

}  // namespace optolink
}  // namespace esphome

#endif
