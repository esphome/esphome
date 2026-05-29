#include "mitsubishi_cn105_vane_select_vertical.h"

#include <array>

namespace esphome::mitsubishi_cn105 {

// NOTE: This order must match VERTICAL_VANE_DIRECTIONS in climate.py.
// MitsubishiCN105VerticalVaneDirectionSelect uses the preferred index-based
// Select API, so Python option order and this array must stay aligned.
static constexpr std::array VALUES{
    VERTICAL_VANE_MODE_AUTO,       VERTICAL_VANE_MODE_POSITION_1, VERTICAL_VANE_MODE_POSITION_2,
    VERTICAL_VANE_MODE_POSITION_3, VERTICAL_VANE_MODE_POSITION_4, VERTICAL_VANE_MODE_POSITION_5,
    VERTICAL_VANE_MODE_SWING,
};

void MitsubishiCN105VerticalVaneDirectionSelect::control(size_t index) {
  if (index < VALUES.size()) {
    this->parent_->set_vertical_vane_direction(VALUES[index]);
  }
}

void MitsubishiCN105VerticalVaneDirectionSelect::publish_vane_state(VerticalVaneMode mode) {
  for (size_t i = 0; i < VALUES.size(); ++i) {
    if (VALUES[i] == mode) {
      this->publish_state(i);
      return;
    }
  }
}

}  // namespace esphome::mitsubishi_cn105
