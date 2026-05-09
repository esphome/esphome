#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

#include "../rojaflex.h"

namespace esphome::rojaflex {

enum class RojaflexBinarySensorType : uint8_t {
  LAST_TX_OK,
};

class RojaflexBinarySensor : public binary_sensor::BinarySensor, public PollingComponent, public RojaflexDevice {
 public:
  void set_sensor_type(RojaflexBinarySensorType type) { this->type_ = type; }
  void update() override;

 protected:
  RojaflexBinarySensorType type_{RojaflexBinarySensorType::LAST_TX_OK};
};

}  // namespace esphome::rojaflex
