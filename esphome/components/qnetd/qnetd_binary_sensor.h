#pragma once
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/qnetd/qnetd.h"
#include "esphome/core/component.h"

namespace esphome::qnetd {

/// ON while some cluster node holds the arbiter's vote.
class QnetdVoteGrantedBinarySensor final : public binary_sensor::BinarySensor, public Component {
 public:
  explicit QnetdVoteGrantedBinarySensor(Qnetd *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

 protected:
  Qnetd *parent_;
};

}  // namespace esphome::qnetd
#endif  // USE_BINARY_SENSOR
