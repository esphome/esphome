#include "seesaw_temperature.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.temperature");

void SeesawTemperature::dump_config() {
  LOG_SENSOR("", "Seesaw Temperature", this);
  LOG_UPDATE_INTERVAL(this);
}

void SeesawTemperature::setup() {
  if (this->parent_->is_failed()) {
    this->mark_failed(LOG_STR("Seesaw hub failed"));
    return;
  }
}

void SeesawTemperature::update() {
  float value = this->parent_->get_temperature();
  this->publish_state(value);
}

}  // namespace esphome::seesaw
