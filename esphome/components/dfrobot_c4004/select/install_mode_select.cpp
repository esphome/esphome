#include "install_mode_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_c4004 {

static const char *const TAG = "dfrobot_c4004.select";

void C4004InstallModeSelect::control(const std::string &value) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (value != "Side" && value != "Top") {
    ESP_LOGW(TAG, "Unsupported install mode: %s", value.c_str());
    return;
  }
  this->parent_->set_pending_install_mode(value);
  this->publish_state(value);
}

}  // namespace dfrobot_c4004
}  // namespace esphome
