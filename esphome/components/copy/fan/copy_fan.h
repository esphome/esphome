#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace copy {

class CopyFan : public fan::Fan, public Component {
 public:
  void set_source(fan::Fan *source) { source_ = source; }
  void setup() override;
  void dump_config() override;

  fan::FanTraits get_traits() override;

 protected:
  void control(const fan::FanCall &call) override;
  ;

  fan::Fan *source_;
  FixedVector<std::string> preset_modes_{};
};

}  // namespace copy
}  // namespace esphome
