#pragma once

#include "../emc2303.h"
#include "esphome/components/output/float_output.h"

namespace esphome::emc2303 {

// This class allows to control the EMC2303 output.
class Emc2303Output : public output::FloatOutput {
 public:
  Emc2303Output(Emc2303Component *parent) : parent_(parent) {}

  /** Set the fan number for this output
   *
   * @param fan The fan number that this output controls
   */
  void set_fan(uint8_t fan) { this->fan_ = fan; }

  uint8_t fan_;

 protected:
  /** Used by ESPHome framework. */
  void write_state(float state) override;

  Emc2303Component *parent_;
};

}  // namespace esphome::emc2303
