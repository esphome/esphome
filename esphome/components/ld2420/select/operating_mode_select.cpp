#include "operating_mode_select.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ld2420 {

static const char *const TAG = "ld2420.select";

void LD2420Select::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_operating_mode(this->option_at(index));
}

}  // namespace esphome::ld2420
