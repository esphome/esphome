#pragma once

#ifdef USE_ESP32
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/fendt_caravan/caravan_sensor_base.h"
#include "esphome/components/fendt_caravan/variable.h"

namespace esphome::fendt_caravan {

class FendtSensor : public CaravanSensorBase<float>, public sensor::Sensor {
 public:
 protected:
  void on_decoded(const float value) override { this->publish_state(value); }

 private:
};
}  // namespace esphome::fendt_caravan
#endif
