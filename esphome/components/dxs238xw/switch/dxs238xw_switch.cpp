#include "dxs238xw_switch.h"

#include "esphome/core/log.h"

namespace esphome {
namespace dxs238xw {

static const char *const TAG = "dxs238xw.switch";

void Dxs238xwSwitch::write_state(bool state) {
  if (this->state != state) {
    this->parent_->set_switch_value(this->entity_id_, state);
  } else {
    ESP_LOGD(TAG, "* Switch not sending unchanged value %s:", ONOFF(state));
  }
}

}  // namespace dxs238xw
}  // namespace esphome
