// Stub for benchmark builds — provides the minimal interface that
// api_connection.cpp and Application need when USE_INFRARED is defined,
// without pulling in the real remote_base/RMT dependencies.
#pragma once

#include <cstdint>
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"

namespace esphome::infrared {

class Infrared;

class InfraredCall {
 public:
  explicit InfraredCall(Infrared *parent) : parent_(parent) {}
  InfraredCall &set_carrier_frequency(uint32_t /*frequency*/) { return *this; }
  InfraredCall &set_raw_timings_packed(const uint8_t * /*data*/, uint16_t /*length*/, uint16_t /*count*/) {
    return *this;
  }
  InfraredCall &set_repeat_count(uint32_t /*count*/) { return *this; }
  void perform() {}

 protected:
  Infrared *parent_;
};

class InfraredTraits {
 public:
  uint32_t get_receiver_frequency_hz() const { return 0; }
};

class Infrared : public Component, public EntityBase {
 public:
  Infrared() = default;
  InfraredTraits &get_traits() { return this->traits_; }
  const InfraredTraits &get_traits() const { return this->traits_; }
  InfraredCall make_call() { return InfraredCall(this); }
  uint32_t get_capability_flags() const { return 0; }

 protected:
  InfraredTraits traits_;
};

}  // namespace esphome::infrared
