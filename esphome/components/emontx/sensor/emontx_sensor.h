#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace emontx {
class EmonTxSensor : public sensor::Sensor, public Component {
 public:
  void dump_config() override;
};

}  // namespace emontx
}  // namespace esphome
