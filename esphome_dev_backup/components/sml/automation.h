#pragma once

#include "esphome/core/automation.h"
#include "sml.h"

#include <vector>

namespace esphome {
namespace sml {

class DataTrigger : public Trigger<const std::vector<uint8_t> &, bool> {
 public:
  explicit DataTrigger(Sml *sml) {
    sml->add_on_data_callback([this](const std::vector<uint8_t> &data, bool valid) { this->trigger(data, valid); });
  }
};

}  // namespace sml
}  // namespace esphome
