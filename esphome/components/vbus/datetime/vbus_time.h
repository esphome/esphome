#pragma once

#include "../resol_deltasol_cs_plus.h"
#ifdef USE_VBUS_SETTING_TIME
#include "esphome/components/datetime/time_entity.h"

namespace esphome::vbus {

class VBusSettingTime final : public datetime::TimeEntity, public CSPlusSettingListener {
 public:
  VBusSettingTime(ResolDeltaSolCSPlus *controller, uint16_t index) : controller_(controller), index_(index) {
    this->hour_ = this->minute_ = this->second_ = 0;
  }
  void dump_setting_config() override;
  void on_setting_snapshot() override;

 protected:
  void control(const datetime::TimeCall &call) override;
  ResolDeltaSolCSPlus *controller_;
  uint16_t index_;
};

}  // namespace esphome::vbus
#endif
