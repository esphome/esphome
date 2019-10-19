#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace tcl112 {

// Temperature
const float TCL112_TEMP_MAX = 31.0;
const float TCL112_TEMP_MIN = 16.0;

class Tcl112Climate : public climate::ClimateIR {
 public:
  Tcl112Climate() : climate::ClimateIR(TCL112_TEMP_MIN, TCL112_TEMP_MAX, .5f) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace tcl112
}  // namespace esphome
