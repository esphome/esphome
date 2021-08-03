#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace toshiba {

const float TOSHIBA_TEMP_MIN = 17.0;
const float TOSHIBA_TEMP_MAX = 30.0;

class ToshibaClimate : public climate_ir::ClimateIR {
 public:
  ToshibaClimate() : climate_ir::ClimateIR(TOSHIBA_TEMP_MIN, TOSHIBA_TEMP_MAX, 1.0f) {}

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

} /* namespace toshiba */
} /* namespace esphome */
