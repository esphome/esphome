#include "toggle_roll_direction_button.h"

namespace esphome {
namespace fyrtur_motor {

void ToggleRollDirectionButton::press_action() { this->parent_->toggle_roll_direction(); }

}  // namespace fyrtur_motor
}  // namespace esphome
