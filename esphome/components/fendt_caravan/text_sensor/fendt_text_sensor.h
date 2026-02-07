#pragma once

#ifdef USE_ESP32

#include "esphome/components/fendt_caravan/caravan_sensor_base.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/fendt_caravan/variable.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"

namespace esphome::fendt_caravan {

using namespace std;

class FendtTextSensor : public CaravanSensorBase<std::string>, public text_sensor::TextSensor {
 public:
 protected:
  void on_decoded(const std::string value) override { this->publish_state(value); }

 private:
};
}  // namespace esphome::fendt_caravan
#endif
