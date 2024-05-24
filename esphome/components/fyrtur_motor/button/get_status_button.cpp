#include "get_status_button.h"

namespace esphome {
namespace fyrtur_motor {

void GetStatusButton::press_action() { this->parent_->get_status(); }

}  // namespace fyrtur_motor
}  // namespace esphome
