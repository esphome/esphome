#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"

namespace esphome::copy {

class CopySelect final : public select::Select, public Component {
  SUB_SELECT(source)

 public:
  void setup() override;
  void dump_config() override;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::copy
