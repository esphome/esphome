#pragma once

#include <optional>

#include "esphome/components/climate/climate.h"
#include "mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105 {

class SwingModeManager final {
 public:
  const climate::ClimateSwingModeMask &supported_swing_modes() const { return this->supported_swing_modes_; }
  void set_supported_swing_modes(const climate::ClimateSwingModeMask &supported_swing_modes) {
    this->supported_swing_modes_ = supported_swing_modes;
  }

  std::optional<MitsubishiCN105::VaneMode> vane_from(climate::ClimateSwingMode swing_mode) const {
    if (!this->supported_swing_modes_.count(climate::CLIMATE_SWING_VERTICAL)) {
      return std::nullopt;
    }

    switch (swing_mode) {
      case climate::CLIMATE_SWING_BOTH:
      case climate::CLIMATE_SWING_VERTICAL:
        return MitsubishiCN105::VaneMode::SWING;
      default:
        return this->last_non_swing_vane_mode_;
    }
  }

  std::optional<MitsubishiCN105::WideVaneMode> wide_vane_from(climate::ClimateSwingMode swing_mode) const {
    if (!this->supported_swing_modes_.count(climate::CLIMATE_SWING_HORIZONTAL)) {
      return std::nullopt;
    }

    switch (swing_mode) {
      case climate::CLIMATE_SWING_BOTH:
      case climate::CLIMATE_SWING_HORIZONTAL:
        return MitsubishiCN105::WideVaneMode::SWING;
      default:
        return this->last_non_swing_wide_vane_mode_;
    }
  }

  std::optional<climate::ClimateSwingMode> update_and_get_swing_mode(MitsubishiCN105::VaneMode vane_mode,
                                                                     MitsubishiCN105::WideVaneMode wide_vane_mode) {
    if (this->supported_swing_modes_.empty()) {
      return std::nullopt;
    }

    bool vertical_swinging = false;
    bool horizontal_swinging = false;
    if (this->supported_swing_modes_.count(climate::CLIMATE_SWING_VERTICAL)) {
      if (vane_mode == MitsubishiCN105::VaneMode::SWING) {
        vertical_swinging = true;
      } else if (vane_mode != MitsubishiCN105::VaneMode::UNKNOWN) {
        this->last_non_swing_vane_mode_ = vane_mode;
      }
    }
    if (this->supported_swing_modes_.count(climate::CLIMATE_SWING_HORIZONTAL)) {
      if (wide_vane_mode == MitsubishiCN105::WideVaneMode::SWING) {
        horizontal_swinging = true;
      } else if (wide_vane_mode != MitsubishiCN105::WideVaneMode::UNKNOWN) {
        this->last_non_swing_wide_vane_mode_ = wide_vane_mode;
      }
    }

    if (vertical_swinging && horizontal_swinging) {
      return climate::CLIMATE_SWING_BOTH;
    }
    if (vertical_swinging) {
      return climate::CLIMATE_SWING_VERTICAL;
    }
    if (horizontal_swinging) {
      return climate::CLIMATE_SWING_HORIZONTAL;
    }
    return climate::CLIMATE_SWING_OFF;
  }

 private:
  climate::ClimateSwingModeMask supported_swing_modes_{};
  MitsubishiCN105::VaneMode last_non_swing_vane_mode_{MitsubishiCN105::VaneMode::AUTO};
  MitsubishiCN105::WideVaneMode last_non_swing_wide_vane_mode_{MitsubishiCN105::WideVaneMode::CENTER};
};

}  // namespace esphome::mitsubishi_cn105
