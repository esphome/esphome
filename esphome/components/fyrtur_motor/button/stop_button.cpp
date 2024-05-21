#include "stop_button.hpp"

namespace esphome {
namespace fyrtur_motor {

void StopButton::press_action() { this->parent_->stop(); }

}  // namespace fyrtur_motor
}  // namespace esphome
