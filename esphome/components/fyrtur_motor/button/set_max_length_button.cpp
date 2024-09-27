#include "set_max_length_button.h"

namespace esphome {
namespace fyrtur_motor {

void SetMaxLengthButton::press_action() { this->parent_->set_max_length(); }

}  // namespace fyrtur_motor
}  // namespace esphome
