#pragma once

#include "../resol_deltasol_cs_plus.h"
#ifdef USE_VBUS_CS_PLUS_ENTITIES
#include "esphome/components/select/select.h"

namespace esphome::vbus {

class VBusSettingSelect final : public select::Select, public CSPlusSettingListener {
 public:
  VBusSettingSelect(ResolDeltaSolCSPlus *controller, uint16_t index, uint8_t minimum)
      : controller_(controller), index_(index), minimum_(minimum) {}
  void initialize_options();
  void dump_setting_config() override;
  void on_setting_snapshot() override;

 protected:
  void control(size_t index) override;
  ResolDeltaSolCSPlus *controller_;
  ConstVector<const char *> labels_;
  FixedVector<const char *> options_;
  uint32_t option_mask_{0};
  uint16_t index_;
  uint8_t minimum_;
};

}  // namespace esphome::vbus
#endif
