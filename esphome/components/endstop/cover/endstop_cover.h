#pragma once

#include "esphome/components/cover/cover.h"
#include "../endstop_actuator.h"

namespace esphome::endstop {

struct EndstopCoverTypes {
  using Entity = cover::Cover;
  using Call = cover::CoverCall;
  using Operation = cover::CoverOperation;
  static constexpr Operation IDLE = cover::COVER_OPERATION_IDLE;
  static constexpr Operation OPENING = cover::COVER_OPERATION_OPENING;
  static constexpr Operation CLOSING = cover::COVER_OPERATION_CLOSING;
  static constexpr const char *TAG = "endstop.cover";
};

class EndstopCover final : public EndstopActuator<EndstopCoverTypes> {
 public:
  void dump_config() override;
  cover::CoverTraits get_traits() override;
};

}  // namespace esphome::endstop
