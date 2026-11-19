#pragma once

#include "esphome/components/sensor/sensor.h"

#include "../duco.h"

namespace esphome::duco {

/// What a sensor reads; the request and where the value is in the answer are in READINGS in sensor.cpp.
enum class DucoReading : uint8_t {
  DUCO_READING_CO2,
  DUCO_READING_NODE_TEMPERATURE,
  DUCO_READING_HUMIDITY,
  DUCO_READING_FILTER_REMAINING,
  DUCO_READING_FLOW_LEVEL,
  DUCO_READING_TIME_REMAINING,
  DUCO_READING_BOX_TEMPERATURE,
  DUCO_READING_BYPASS,
};

class DucoSensor : public DucoDevice, public sensor::Sensor {
 public:
  /// argument is the node address, or the parameter number for a box parameter.
  DucoSensor(DucoReading reading, uint8_t argument) : reading_(reading), argument_(argument) {}

  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  DucoReading reading_;
  uint8_t argument_;
};

}  // namespace esphome::duco
