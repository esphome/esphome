#include "seesaw_temperature.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.temperature");

void SeesawTemperature::dump_config() {
  LOG_SENSOR("", "Seesaw Temperature", this);
  LOG_UPDATE_INTERVAL(this);
}

void SeesawTemperature::update() {
  float value = this->parent_->get_temperature();
  this->publish_state(value);
}

}  // namespace esphome::seesaw
