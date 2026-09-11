// Stub for benchmark builds — provides the minimal interface that
// api_connection.cpp and Application need when USE_RADIO_FREQUENCY is defined.
#pragma once

#include <cstdint>
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"

namespace esphome::radio_frequency {

enum RadioFrequencyModulation : uint32_t {
  RADIO_FREQUENCY_MODULATION_OOK = 0,
};

class RadioFrequency;

class RadioFrequencyCall {
 public:
  explicit RadioFrequencyCall(RadioFrequency *parent) : parent_(parent) {}
  RadioFrequencyCall &set_frequency(uint32_t /*frequency*/) { return *this; }
  RadioFrequencyCall &set_modulation(RadioFrequencyModulation /*mod*/) { return *this; }
  RadioFrequencyCall &set_repeat_count(uint32_t /*count*/) { return *this; }
  RadioFrequencyCall &set_raw_timings_packed(const uint8_t * /*data*/, uint16_t /*length*/, uint16_t /*count*/) {
    return *this;
  }
  void perform() {}

 protected:
  RadioFrequency *parent_;
};

class RadioFrequencyTraits {
 public:
  uint32_t get_frequency_min_hz() const { return 0; }
  uint32_t get_frequency_max_hz() const { return 0; }
  uint32_t get_supported_modulations() const { return 0; }
};

class RadioFrequency : public Component, public EntityBase {
 public:
  RadioFrequency() = default;
  RadioFrequencyTraits &get_traits() { return this->traits_; }
  const RadioFrequencyTraits &get_traits() const { return this->traits_; }
  RadioFrequencyCall make_call() { return RadioFrequencyCall(this); }
  uint32_t get_capability_flags() const { return 0; }

 protected:
  RadioFrequencyTraits traits_;
};

}  // namespace esphome::radio_frequency
