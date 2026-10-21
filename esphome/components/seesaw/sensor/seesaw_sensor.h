#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

enum class SeesawSensorType : uint8_t {
  SEESAW_SENSOR_TYPE_ADC,
  SEESAW_SENSOR_TYPE_TEMPERATURE,
  SEESAW_SENSOR_TYPE_TOUCH,
};

/// An ADC pin, a touch channel or the chip temperature, read once per update interval.
class SeesawSensor : public sensor::Sensor, public PollingComponent, public Parented<Seesaw> {
 public:
  SeesawSensor(SeesawSensorType type, uint8_t channel) : type_(type), channel_(channel) {}

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  const SeesawSensorType type_;
  // The ADC pin or touch channel
  const uint8_t channel_;
};

}  // namespace esphome::seesaw
