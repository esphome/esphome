#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace copy {

class CopyCover : public cover::Cover, public Component {
 public:
  void set_source(cover::Cover *source) { source_ = source; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;

  cover::Cover *source_;
};

}  // namespace copy
}  // namespace esphome
