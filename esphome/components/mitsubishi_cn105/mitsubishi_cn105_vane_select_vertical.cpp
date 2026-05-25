#include "mitsubishi_cn105_vane_select_vertical.h"

namespace esphome::mitsubishi_cn105 {

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
  if (const auto *const it = std::find(VALUES.begin(), VALUES.end(), mode); it != VALUES.end()) {
    this->publish_state(std::distance(VALUES.begin(), it));
  }
}

}  // namespace esphome::mitsubishi_cn105
