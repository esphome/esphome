#include "operating_mode_select.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <string>
#include <vector>

namespace esphome {
namespace dfrobot_c4002 {

void C4002Select::control(const std::string &value) {
  if (this->parent_) {
    for (int i = 0; i < 3; i++) {
      if (value == options[i]) {
        ESP_LOGW(TAG, "set output mode to %s", options[i].c_str());
        this->parent_->setOutMode((eOutMode_t) (i + 1));
        this->publish_state(value);
      }
    }
  }
}

}  // namespace dfrobot_c4002
}  // namespace esphome
