#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/ira211_protocol.h"

namespace esphome::climate_ir_siemens_ira211 {

class SiemensIRA211Climate final : public climate_ir::ClimateIR {
 public:
  void setup() override;
  SiemensIRA211Climate()
      : climate_ir::ClimateIR(5.0f, 35.0f, 0.5f, false, false,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {}, {climate::CLIMATE_PRESET_COMFORT, climate::CLIMATE_PRESET_ECO}) {}

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void transmit_frame_(remote_base::IRA211Command command, remote_base::IRA211Mode mode, remote_base::IRA211Fan fan);
  void apply_mode_(remote_base::IRA211Mode mode);
  climate::ClimateMode active_mode_() const;

  bool is_on_{false};
};

}  // namespace esphome::climate_ir_siemens_ira211
