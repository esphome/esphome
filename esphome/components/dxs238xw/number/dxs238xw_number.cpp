#include "dxs238xw_number.h"

#include "esphome/core/log.h"

namespace esphome {
namespace dxs238xw {

static const char *const TAG = "dxs238xw.number";

void Dxs238xwNumber::control(float value) {
  if (this->state != value) {
    this->parent_->set_number_value(this->entity_id_, value);
  } else {
    ESP_LOGD(TAG, "* Number not sending unchanged value %f:", value);
  }
}

}  // namespace dxs238xw
}  // namespace esphome
