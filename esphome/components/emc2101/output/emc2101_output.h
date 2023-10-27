#pragma once

#include "../emc2101.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace emc2101 {

/// This class allows to control the EMC2101 output.
class EMC2101Output : public output::FloatOutput {
 public:
  EMC2101Output(Emc2101Component *parent) : parent_(parent) {}

 protected:
  /** Used by ESPHome framework. */
  void write_state(float state) override;

  Emc2101Component *parent_;
};

}  // namespace emc2101
}  // namespace esphome
