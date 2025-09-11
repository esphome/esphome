#include "operating_mode_select.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_c4001 {

static const char *const TAG = "dfrobot_c4001.select";

void C4001Select::control(const std::string &value) {
  if (this->parent_) {
    this->parent_->set_operating_mode(value);
    this->publish_state(value);
  }
}

}  // namespace dfrobot_c4001
}  // namespace esphome
