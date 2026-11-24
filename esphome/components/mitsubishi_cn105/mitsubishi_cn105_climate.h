#pragma once

#include "mitsubishi_cn105_component.h"
#include "mitsubishi_cn105.h"

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "mitsubishi_cn105_swing_mode_manager.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105Climate final : public climate::Climate,
                                     public Component,
                                     public Parented<MitsubishiCN105Component> {
 public:
  void setup() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_supported_swing_mode(climate::ClimateSwingMode mode);
  // Legacy climate action compatibility. Remove in 2027.2.0.
  void set_remote_temperature(float temperature) { this->parent_->set_remote_temperature(temperature); }
  void clear_remote_temperature() { this->parent_->clear_remote_temperature(); }

 protected:
  void apply_values_();

  SwingModeManager swing_mode_manager_;
};

}  // namespace esphome::mitsubishi_cn105
