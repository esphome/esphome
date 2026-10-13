#pragma once

#include "esphome/core/automation.h"
#include "microphone.h"

#include <cstdint>
#include <vector>

namespace esphome::microphone {

/// @brief Fires an on_data automation with each capture as a std::vector in `x`.
///
/// The vector is reused for every buffer, so it only allocates while it grows. Holding the data instead of a view of
/// the microphone's buffer keeps `x` valid in automations that copy it, such as ones with a delay.
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
