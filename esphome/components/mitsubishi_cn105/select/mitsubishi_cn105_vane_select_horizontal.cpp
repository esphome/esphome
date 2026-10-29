#include "mitsubishi_cn105_vane_select_horizontal.h"

#include "esphome/core/hal.h"

#include <array>

namespace esphome::mitsubishi_cn105 {

// NOTE: This order must match HORIZONTAL_VANE_DIRECTIONS in select/__init__.py.
// MitsubishiCN105HorizontalVaneDirectionSelect uses the preferred index-based
// Select API, so Python option order and this array must stay aligned.
static constexpr std::array VALUES PROGMEM{
    MitsubishiCN105::WideVaneMode::FAR_LEFT,  MitsubishiCN105::WideVaneMode::LEFT,
    MitsubishiCN105::WideVaneMode::CENTER,    MitsubishiCN105::WideVaneMode::RIGHT,
    MitsubishiCN105::WideVaneMode::FAR_RIGHT, MitsubishiCN105::WideVaneMode::LEFT_RIGHT,
    MitsubishiCN105::WideVaneMode::SWING,
};

static_assert(sizeof(MitsubishiCN105::WideVaneMode) == 1, "VALUES is read byte-wise from PROGMEM");

static MitsubishiCN105::WideVaneMode value_at(size_t index) {
  return static_cast<MitsubishiCN105::WideVaneMode>(
      progmem_read_byte(reinterpret_cast<const uint8_t *>(&VALUES[index])));
}

void MitsubishiCN105HorizontalVaneDirectionSelect::setup() {
  this->parent_->add_on_status_callback(
      [this]() { this->publish_wide_vane_state(this->parent_->status().wide_vane_mode); });
  if (this->parent_->is_status_initialized()) {
    this->publish_wide_vane_state(this->parent_->status().wide_vane_mode);
  }
}

void MitsubishiCN105HorizontalVaneDirectionSelect::control(size_t index) {
  if (index < VALUES.size()) {
    this->parent_->set_wide_vane_mode(value_at(index));
    this->parent_->publish_status();
  }
}

void MitsubishiCN105HorizontalVaneDirectionSelect::publish_wide_vane_state(MitsubishiCN105::WideVaneMode mode) {
  for (size_t i = 0; i < VALUES.size(); ++i) {
    if (value_at(i) == mode) {
      this->publish_state(i);
      return;
    }
  }
}

}  // namespace esphome::mitsubishi_cn105
