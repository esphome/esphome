#include "move_up_button.h"

namespace esphome {
namespace fyrtur_motor {

void MoveUpButton::press_action() { this->parent_->move_up(); }

}  // namespace fyrtur_motor
}  // namespace esphome
