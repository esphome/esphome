#pragma once

#include "esphome/core/automation.h"
#include "microphone.h"

#include <cstdint>
#include <vector>

namespace esphome::microphone {

/// Reuses one vector so `x` stays valid in automations with a delay.
class DataTrigger final : public Trigger<const std::vector<uint8_t> &> {
 public:
  explicit DataTrigger(Microphone *parent) {
    parent->add_data_callback([this](const AudioCapture &capture) {
      this->data_.assign(capture.data.begin(), capture.data.end());
      this->trigger(this->data_);
    });
  }

 protected:
  std::vector<uint8_t> data_;
};

}  // namespace esphome::microphone
