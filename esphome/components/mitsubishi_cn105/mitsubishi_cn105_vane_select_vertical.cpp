#include "mitsubishi_cn105_vane_select_vertical.h"

#include <array>

namespace esphome::mitsubishi_cn105 {

// NOTE: This order must match VERTICAL_VANE_DIRECTIONS in climate.py.
// MitsubishiCN105VerticalVaneDirectionSelect uses the preferred index-based
// Select API, so Python option order and this array must stay aligned.
static constexpr std::array VALUES{
    MitsubishiCN105::VaneMode::AUTO,       MitsubishiCN105::VaneMode::POSITION_1, MitsubishiCN105::VaneMode::POSITION_2,
    MitsubishiCN105::VaneMode::POSITION_3, MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::VaneMode::POSITION_5,
    MitsubishiCN105::VaneMode::SWING,
};

void MitsubishiCN105VerticalVaneDirectionSelect::control(size_t index) {
  if (index < VALUES.size()) {
    this->parent_->set_vertical_vane_direction(VALUES[index]);
  }
}

void MitsubishiCN105VerticalVaneDirectionSelect::publish_vane_state(MitsubishiCN105::VaneMode mode) {
  for (size_t i = 0; i < VALUES.size(); ++i) {
    if (VALUES[i] == mode) {
      this->publish_state(i);
      return;
    }
  }
}

}  // namespace esphome::mitsubishi_cn105
