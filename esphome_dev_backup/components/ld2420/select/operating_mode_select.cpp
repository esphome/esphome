#include "operating_mode_select.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2420 {

static const char *const TAG = "LD2420.select";

void LD2420Select::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_operating_mode(value);
}

}  // namespace ld2420
}  // namespace esphome
