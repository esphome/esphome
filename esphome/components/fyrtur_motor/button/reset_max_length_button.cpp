#include "reset_max_length_button.h"

namespace esphome {
namespace fyrtur_motor {

void ResetMaxLengthButton::press_action() { this->parent_->reset_max_length(); }

}  // namespace fyrtur_motor
}  // namespace esphome
