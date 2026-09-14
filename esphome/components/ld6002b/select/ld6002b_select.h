#pragma once

#include "esphome/components/select/select.h"
#include "../ld6002b.h"

namespace esphome::ld6002b {

class LD6002BSelect : public select::Select, public Parented<LD6002BComponent> {
 public:
  explicit LD6002BSelect(SelectType type) : type_(type) {}

 protected:
  void control(size_t index) override;

  SelectType type_;
};

}  // namespace esphome::ld6002b
