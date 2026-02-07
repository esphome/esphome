#pragma once

#ifdef USE_ESP32
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/fendt_caravan/fendt_component.h"
#include "esphome/components/fendt_caravan/variable.h"

namespace esphome::fendt_caravan {

#define FENDT_SENSOR(name) \
 protected: \
  FendtSensor *name##_sensor_{nullptr}; \
\
 public: \
  void set_##name##_sensor(FendtSensor *sensor) { this->name##_sensor_ = sensor; }

class FendtSensor : public FendtComponent<float>, public sensor::Sensor {
 public:
 protected:
  void on_decoded(const float value) override { this->publish_state(value); }

 private:
};
}  // namespace esphome::fendt_caravan
#endif
