#include "mitsubishi_cn105_swing_mode_manager.h"

namespace esphome::mitsubishi_cn105 {

std::optional<MitsubishiCN105::VaneMode> SwingModeManager::vane_from(climate::ClimateSwingMode swing_mode) const {
  if (!this->supported_swing_modes_.count(climate::CLIMATE_SWING_VERTICAL)) {
    return std::nullopt;
  }

  switch (swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
    case climate::CLIMATE_SWING_VERTICAL:
      return MitsubishiCN105::VaneMode::SWING;

    case climate::CLIMATE_SWING_HORIZONTAL:
    case climate::CLIMATE_SWING_OFF:
    default:
      return this->last_non_swing_vane_mode_;
  }
}

std::optional<MitsubishiCN105::WideVaneMode> SwingModeManager::wide_vane_from(
    climate::ClimateSwingMode swing_mode) const {
  if (!this->supported_swing_modes_.count(climate::CLIMATE_SWING_HORIZONTAL)) {
    return std::nullopt;
  }

  switch (swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
    case climate::CLIMATE_SWING_HORIZONTAL:
      return MitsubishiCN105::WideVaneMode::SWING;

    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_OFF:
    default:
      return this->last_non_swing_wide_vane_mode_;
  }
}

std::optional<climate::ClimateSwingMode> SwingModeManager::swing_mode_from(
    MitsubishiCN105::VaneMode vane_mode, MitsubishiCN105::WideVaneMode wide_vane_mode) {
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

}  // namespace esphome::mitsubishi_cn105
