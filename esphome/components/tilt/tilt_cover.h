#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace tilt {

class TiltCover : public cover::Cover, public Component {
 public:
  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override;

  Trigger<> *get_tilt_trigger() const { return this->tilt_trigger_; }
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;

  Trigger<> *tilt_trigger_{new Trigger<>()};
};

}  // namespace tilt
}  // namespace esphome
