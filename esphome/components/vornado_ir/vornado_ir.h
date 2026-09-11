#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include <functional>

namespace esphome::vornado_ir {

using remote_base::RemoteTransmitterBase;
using remote_base::RawTimings;

class VornadoIR : public Component, public remote_base::RemoteTransmittable {
 public:
  void dump_config() override;
  // general functions
  void transmit_(const RawTimings &ir_code);
  // direct actions
  void send_power_toggle();
  void send_change_direction();
  void send_increase();
  void send_decrease();
};

}  // namespace esphome::vornado_ir
