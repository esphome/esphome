#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/sensor/sensor.h"
#include "../optolink.h"
#include "../datapoint_component.h"
#include <cfloat>

namespace esphome {
namespace optolink {

enum SensorType { SENSOR_TYPE_DATAPOINT, SENSOR_TYPE_QUEUE_SIZE };

class OptolinkSensor : public DatapointComponent, public esphome::sensor::Sensor {
 public:
  OptolinkSensor(Optolink *optolink) : DatapointComponent(optolink) {
    set_state_class(esphome::sensor::STATE_CLASS_MEASUREMENT);
  }

  void set_type(SensorType type) { type_ = type; }
  void set_min_value(float min_value) { min_value_ = min_value; }

 protected:
  void setup() override;
  void update() override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(float value) override { publish_state(value); };
  void datapoint_value_changed(uint8_t value) override;
  void datapoint_value_changed(uint16_t value) override;
  void datapoint_value_changed(uint32_t value) override;

 private:
  SensorType type_ = SENSOR_TYPE_DATAPOINT;
  float min_value_ = -FLT_MAX;
};
}  // namespace optolink
}  // namespace esphome

#endif
