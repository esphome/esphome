#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::copy {

class CopyBinarySensor final : public binary_sensor::BinarySensor, public Component {
  SUB_BINARY_SENSOR(source)

 public:
  void setup() override;
  void dump_config() override;
};

}  // namespace esphome::copy
