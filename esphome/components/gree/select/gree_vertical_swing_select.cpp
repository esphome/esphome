#include "gree_vertical_swing_select.h"

#include "esphome/core/log.h"

namespace esphome::gree {

ESPHOME_LOG_TAG(TAG, "gree.select");

// Keep this order in sync with VERTICAL_SWING_OPTIONS in __init__.py.
static constexpr uint8_t GREE_VERTICAL_SWING_POSITIONS[] = {
    GREE_VDIR_AUTO,          // Default
    GREE_VDIR_SWING,         // Full Swing
    GREE_VDIR_UP,            // Fixed Upper
    GREE_VDIR_MUP,           // Fixed Upper Middle
    GREE_VDIR_MIDDLE,        // Fixed Middle
    GREE_VDIR_MDOWN,         // Fixed Lower Middle
    GREE_VDIR_DOWN,          // Fixed Lower
    GREE_VDIR_SWING_DOWN,    // Swing Lower
    GREE_VDIR_SWING_MIDDLE,  // Swing Middle
    GREE_VDIR_SWING_UP,      // Swing Upper
};
static constexpr size_t GREE_VERTICAL_SWING_COUNT =
    sizeof(GREE_VERTICAL_SWING_POSITIONS) / sizeof(GREE_VERTICAL_SWING_POSITIONS[0]);

void GreeVerticalSwingSelect::setup() {
  this->parent_->register_vertical_swing_listener(this);
  this->on_vertical_swing_position(this->parent_->get_vertical_swing_position());
}

void GreeVerticalSwingSelect::dump_config() {
  LOG_SELECT(TAG, "Gree Vertical Swing", this);
  ESP_LOGCONFIG(TAG, "  Vane position: 0x%02X", this->parent_->get_vertical_swing_position());
}

void GreeVerticalSwingSelect::control(size_t index) {
  if (index >= GREE_VERTICAL_SWING_COUNT)
    return;
  // The climate component transmits the frame and notifies us, which publishes the new index.
  this->parent_->set_vertical_swing_position(GREE_VERTICAL_SWING_POSITIONS[index]);
}

void GreeVerticalSwingSelect::on_vertical_swing_position(uint8_t position) {
  for (size_t index = 0; index < GREE_VERTICAL_SWING_COUNT; index++) {
    if (GREE_VERTICAL_SWING_POSITIONS[index] == position) {
      this->publish_state(index);
      return;
    }
  }
}

}  // namespace esphome::gree
