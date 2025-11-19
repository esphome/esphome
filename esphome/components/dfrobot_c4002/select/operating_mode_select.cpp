#include "operating_mode_select.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <string>
#include <vector>

namespace esphome {
namespace dfrobot_c4002 {

static const char *const TAG = "dfrobot_c4002.select: ";

void C4002Select::control(const std::string &value) {
  if (this->parent_) {
    for (int i = 0; i < 3; i++) {
      if (value == options_[i]) {
        ESP_LOGW(TAG, "set output mode to %s", options_[i].c_str());
        this->parent_->set_out_mode((OutMode) (i + 1));
        this->publish_state(value);
      }
    }
  }
}

}  // namespace dfrobot_c4002
}  // namespace esphome
