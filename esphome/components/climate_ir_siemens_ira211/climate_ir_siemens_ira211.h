#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/ira211_protocol.h"

namespace esphome {
namespace climate_ir_siemens_ira211 {

class SiemensIRA211Climate : public climate_ir::ClimateIR {
 public:
  SiemensIRA211Climate()
      : climate_ir::ClimateIR(5.0f, 35.0f, 0.5f, false, false,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {}, {climate::CLIMATE_PRESET_COMFORT, climate::CLIMATE_PRESET_ECO}) {}

 protected:
  climate::ClimateTraits traits() override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void transmit_frame_(remote_base::IRA211Command cmd, remote_base::IRA211Mode mode, remote_base::IRA211Fan fan);

  remote_base::IRA211Mode climate_preset_to_ira211_mode_() const;
  remote_base::IRA211Fan climate_fan_to_ira211_fan_() const;
  void ira211_mode_to_climate_preset_(remote_base::IRA211Mode mode);
  void ira211_fan_to_climate_fan_(remote_base::IRA211Fan fan);
  climate::ClimateMode active_climate_mode_() const;

  bool is_on_{false};
};

}  // namespace climate_ir_siemens_ira211
}  // namespace esphome
