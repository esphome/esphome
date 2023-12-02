#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace copy {

class CopyNumber : public number::Number, public Component {
 public:
  void set_source(number::Number *source) { source_ = source; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;

  number::Number *source_;
};

}  // namespace copy
}  // namespace esphome
