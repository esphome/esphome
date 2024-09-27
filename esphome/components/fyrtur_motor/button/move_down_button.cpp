#include "move_down_button.h"

namespace esphome {
namespace fyrtur_motor {

void MoveDownButton::press_action() { this->parent_->move_down(); }

}  // namespace fyrtur_motor
}  // namespace esphome
