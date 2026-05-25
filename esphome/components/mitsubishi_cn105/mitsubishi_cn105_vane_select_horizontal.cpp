#include "mitsubishi_cn105_vane_select_horizontal.h"

namespace esphome::mitsubishi_cn105 {

static constexpr std::array VALUES{
    MitsubishiCN105::WideVaneMode::FAR_LEFT,  MitsubishiCN105::WideVaneMode::LEFT,
    MitsubishiCN105::WideVaneMode::CENTER,    MitsubishiCN105::WideVaneMode::RIGHT,
    MitsubishiCN105::WideVaneMode::FAR_RIGHT, MitsubishiCN105::WideVaneMode::LEFT_RIGHT,
    MitsubishiCN105::WideVaneMode::SWING,
};

void MitsubishiCN105HorizontalVaneDirectionSelect::control(size_t index) {
  if (index < VALUES.size()) {
    this->parent_->set_horizontal_vane_direction(VALUES[index]);
  }
}

void MitsubishiCN105HorizontalVaneDirectionSelect::publish_vane_state(MitsubishiCN105::WideVaneMode mode) {
  if (const auto *const it = std::find(VALUES.begin(), VALUES.end(), mode); it != VALUES.end()) {
    this->publish_state(std::distance(VALUES.begin(), it));
  }
}

}  // namespace esphome::mitsubishi_cn105
