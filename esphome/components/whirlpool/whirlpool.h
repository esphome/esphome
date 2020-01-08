#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace whirlpool {

// Temperature
const float WHIRLPOOL_TEMP_MAX = 30.0;
const float WHIRLPOOL_TEMP_MIN = 16.0;

class WhirlpoolClimate : public climate_ir::ClimateIR {
 public:
  WhirlpoolClimate()
      : climate_ir::ClimateIR(WHIRLPOOL_TEMP_MIN, WHIRLPOOL_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void setup() override {
    climate_ir::ClimateIR::setup();

    this->powered_on_assumed_ = this->mode != climate::CLIMATE_MODE_OFF;
  }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  // bool on_receive(remote_base::RemoteReceiveData data) override;

  // used to track when to send the power toggle command
  bool powered_on_assumed_;
};

}  // namespace whirlpool
}  // namespace esphome
