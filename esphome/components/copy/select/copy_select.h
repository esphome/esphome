#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace copy {

class CopySelect : public select::Select, public Component {
 public:
  void set_source(select::Select *source) { source_ = source; }
  void setup() override;
  void dump_config() override;

 protected:
  void control(size_t index) override;

  select::Select *source_;
};

}  // namespace copy
}  // namespace esphome
