#pragma once

#include "esphome/components/valve/valve.h"
#include "../endstop_actuator.h"

namespace esphome::endstop {

struct EndstopValveTypes {
  using Entity = valve::Valve;
  using Call = valve::ValveCall;
  using Operation = valve::ValveOperation;
  static constexpr Operation IDLE = valve::VALVE_OPERATION_IDLE;
  static constexpr Operation OPENING = valve::VALVE_OPERATION_OPENING;
  static constexpr Operation CLOSING = valve::VALVE_OPERATION_CLOSING;
  static constexpr const char *TAG = "endstop.valve";
};

class EndstopValve final : public EndstopActuator<EndstopValveTypes> {
 public:
  void dump_config() override;
  valve::ValveTraits get_traits() override;
};

}  // namespace esphome::endstop
