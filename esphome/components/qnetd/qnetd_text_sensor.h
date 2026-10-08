#pragma once
#include "esphome/core/defines.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/qnetd/qnetd.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome::qnetd {

/// Per-cluster summary of the vote held by each node, e.g. "pve: 1=ACK 2=NACK".
class QnetdStatusTextSensor final : public text_sensor::TextSensor, public Component {
 public:
  explicit QnetdStatusTextSensor(Qnetd *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

 protected:
  void publish_();

  Qnetd *parent_;
};

}  // namespace esphome::qnetd
#endif  // USE_TEXT_SENSOR
