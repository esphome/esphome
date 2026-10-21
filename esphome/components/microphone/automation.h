#pragma once

#include "esphome/core/automation.h"
#include "microphone.h"

#include <vector>

namespace esphome::microphone {

class DataTrigger final : public Trigger<const std::vector<uint8_t> &> {
 public:
  explicit DataTrigger(Microphone *mic) {
    mic->add_data_callback([this](const std::vector<uint8_t> &data) { this->trigger(data); });
  }
};

}  // namespace esphome::microphone
