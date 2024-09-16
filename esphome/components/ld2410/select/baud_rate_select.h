#pragma once

#include "esphome/components/select/select.h"
#include "../ld2410.h"

namespace esphome {
namespace ld2410 {

class BaudRateSelect : public select::Select, public Parented<LD2410Component> {
 public:
  BaudRateSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace ld2410
}  // namespace esphome
