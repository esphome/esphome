#pragma once

#include "esphome/components/climate/climate.h"
#include "mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105 {

class SwingModeManager {
 public:
  const climate::ClimateSwingModeMask &supported_swing_modes() const { return this->supported_swing_modes_; }
  void set_supported_swing_modes(climate::ClimateSwingModeMask modes) { this->supported_swing_modes_ = modes; }

  std::optional<MitsubishiCN105::VaneMode> vane_from(climate::ClimateSwingMode swing_mode) const;
  std::optional<MitsubishiCN105::WideVaneMode> wide_vane_from(climate::ClimateSwingMode swing_mode) const;
  std::optional<climate::ClimateSwingMode> swing_mode_from(MitsubishiCN105::VaneMode vane_mode,
                                                           MitsubishiCN105::WideVaneMode wide_vane_mode);

 protected:
  climate::ClimateSwingModeMask supported_swing_modes_{};
  MitsubishiCN105::VaneMode last_non_swing_vane_mode_{MitsubishiCN105::VaneMode::AUTO};
  MitsubishiCN105::WideVaneMode last_non_swing_wide_vane_mode_{MitsubishiCN105::WideVaneMode::CENTER};
};

}  // namespace esphome::mitsubishi_cn105
