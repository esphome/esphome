#pragma once

#ifdef USE_ESP32
#include "esphome/components/fendt_caravan/fendt_caravan_hub_base.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::fendt_caravan {

class FendtSensor : public sensor::Sensor, public Parented<FendtCaravanHubBase> {
 public:
 private:
};
}  // namespace esphome::fendt_caravan
#endif
