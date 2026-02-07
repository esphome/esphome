#pragma once

#ifdef USE_ESP32
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"
#include "esphome/components/fendt_caravan/fendt_component.h"
#include "esphome/components/fendt_caravan/variable.h"

namespace esphome::fendt_caravan {

using namespace std;

#define FENDT_TEXT_SENSOR(name) \
 protected: \
  FendtTextSensor *name##_text_sensor_{nullptr}; \
\
 public: \
  void set_##name##_text_sensor(FendtTextSensor *sensor) { this->name##_text_sensor_ = sensor; }

class FendtTextSensor : public FendtComponent<std::string>, public text_sensor::TextSensor {
 public:
 protected:
  void on_decoded(const std::string value) override { this->publish_state(value); }

 private:
};
}  // namespace esphome::fendt_caravan
#endif
