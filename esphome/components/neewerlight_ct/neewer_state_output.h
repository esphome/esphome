#pragma once

#include "esphome/components/output/float_output.h"

#ifdef USE_ESP32

namespace esphome::neewerlight_ct {

class NeewerStateOutput : public output::FloatOutput {
 protected:
  void write_state(float state) override {
    // do nothing
  }
};

}  // namespace esphome::neewerlight_ct

#endif  // USE_ESP32
