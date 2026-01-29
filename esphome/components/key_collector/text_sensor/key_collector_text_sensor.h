#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../key_collector.h"

namespace esphome {
namespace key_collector {

class KeyCollectorTextSensor : public text_sensor::TextSensor, public Component {
 public:
  KeyCollectorTextSensor(KeyCollector *parent) : parent_(parent) {}

  void setup() override;

 protected:
  KeyCollector *parent_;
};

}  // namespace key_collector
}  // namespace esphome
