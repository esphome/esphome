#pragma once

#ifdef USE_ESP32
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/fendt_caravan/caravan_sensor_base.h"
#include "esphome/components/fendt_caravan/variable.h"

namespace esphome::fendt_caravan {

class FendtSensor : public CaravanSensorBase<float>, public sensor::Sensor {
 public:
  void setup() override {
    if (this->key_name_.empty())
      return;
    auto *variable = static_cast<Variable<float> *>(this->get_parent()->get_variable(this->key_name_));
    if (variable != nullptr) {
      this->set_variable(variable);
    }
  }

 protected:
  void on_decoded(float value) override;

 private:
};
}  // namespace esphome::fendt_caravan
#endif
