#pragma once

#include "esphome/components/select/select.h"
#include "../ld2450.h"

namespace esphome {
namespace ld2450 {

class BaudRateSelect : public select::Select, public Parented<LD2450Component> {
 public:
  BaudRateSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace ld2450
}  // namespace esphome
