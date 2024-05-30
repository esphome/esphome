#include "open_close_switch.h"

namespace esphome {
namespace fyrtur_motor {

void OpenCloseSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->open_close(state);
}

}  // namespace fyrtur_motor
}  // namespace esphome
