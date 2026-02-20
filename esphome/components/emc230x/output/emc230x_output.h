#pragma once

#include "../emc230x.h"
#include "esphome/components/output/float_output.h"

namespace esphome::emc230x {

// This class allows to control the EMC230X outputs.
class Emc230xOutput : public output::FloatOutput {
 public:
  Emc230xOutput(Emc230xComponent *parent) : parent_(parent) {}

  /** Set the fan number for this output
   *
   * @param fan The fan number that this output controls
   */
  void set_fan(uint8_t fan) { this->fan_ = fan; }

  uint8_t fan_;

 protected:
  /** Used by ESPHome framework. */
  void write_state(float state) override;

  Emc230xComponent *parent_;
};

}  // namespace esphome::emc230x
