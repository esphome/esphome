#include "mitsubishi_cn105_vane_select_vertical.h"

#include "esphome/core/hal.h"

#include <array>

namespace esphome::mitsubishi_cn105 {

// NOTE: This order must match VERTICAL_VANE_DIRECTIONS in the hub's __init__.py.
// MitsubishiCN105VerticalVaneDirectionSelect uses the preferred index-based
// Select API, so Python option order and this array must stay aligned.
static constexpr std::array VALUES PROGMEM{
    MitsubishiCN105::VaneMode::AUTO,       MitsubishiCN105::VaneMode::POSITION_1, MitsubishiCN105::VaneMode::POSITION_2,
    MitsubishiCN105::VaneMode::POSITION_3, MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::VaneMode::POSITION_5,
    MitsubishiCN105::VaneMode::SWING,
};

static_assert(sizeof(MitsubishiCN105::VaneMode) == 1, "VALUES is read byte-wise from PROGMEM");

static MitsubishiCN105::VaneMode value_at(size_t index) {
  return static_cast<MitsubishiCN105::VaneMode>(progmem_read_byte(reinterpret_cast<const uint8_t *>(&VALUES[index])));
}

void MitsubishiCN105VerticalVaneDirectionSelect::setup() {
  this->parent_->add_on_status_callback([this]() { this->publish_vane_state(this->parent_->status().vane_mode); });
  if (this->parent_->is_status_initialized()) {
    this->publish_vane_state(this->parent_->status().vane_mode);
  }
}

void MitsubishiCN105VerticalVaneDirectionSelect::control(size_t index) {
  if (index < VALUES.size()) {
    this->parent_->set_vane_mode(value_at(index));
    this->parent_->publish_status();
  }
}

void MitsubishiCN105VerticalVaneDirectionSelect::publish_vane_state(MitsubishiCN105::VaneMode mode) {
  for (size_t i = 0; i < VALUES.size(); ++i) {
    if (value_at(i) == mode) {
      this->publish_state(i);
      return;
    }
  }
}

}  // namespace esphome::mitsubishi_cn105
