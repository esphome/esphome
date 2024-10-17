#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "esphome/components/gp8211/gp8211.h"

namespace esphome {
namespace gp8211 {

class GP8211Output : public Component, public output::FloatOutput, public Parented<GP8211> {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1; }

  void write_state(float state) override;
};

}  // namespace gp8211
}  // namespace esphome
