#pragma once

#include "../resol_deltasol_cs_plus.h"
#ifdef USE_VBUS_CS_PLUS_ENTITIES
#include "esphome/components/number/number.h"

namespace esphome::vbus {

class VBusSettingNumber final : public number::Number, public CSPlusSettingListener {
 public:
  VBusSettingNumber(ResolDeltaSolCSPlus *controller, uint16_t index) : controller_(controller), index_(index) {}
  void dump_setting_config() override;
  void on_setting_snapshot() override;

 protected:
  void control(float value) override;
  ResolDeltaSolCSPlus *controller_;
  uint16_t index_;
};

}  // namespace esphome::vbus
#endif
