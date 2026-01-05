#include "button.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace alpha3 {

static const char *const TAG = "alpha3.button";

void Alpha3Button::press_action() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set");
    return;
  }

  switch (this->action_) {
    case ACTION_START:
      ESP_LOGI(TAG, "Start pump button pressed");
      this->parent_->start_pump();
      break;
    case ACTION_STOP:
      ESP_LOGI(TAG, "Stop pump button pressed");
      this->parent_->stop_pump();
      break;
    case ACTION_SETPOINT_UP:
      ESP_LOGI(TAG, "Setpoint up button pressed");
      this->parent_->adjust_setpoint(1);
      break;
    case ACTION_SETPOINT_DOWN:
      ESP_LOGI(TAG, "Setpoint down button pressed");
      this->parent_->adjust_setpoint(-1);
      break;
  }
}

}  // namespace alpha3
}  // namespace esphome

#endif
