#include "seesaw_temperature.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

static const char *const TAG = "seesaw.temperature";

void SeesawTemperature::setup() { ESP_LOGCONFIG(TAG, "Setting up Seesaw temperature..."); }

void SeesawTemperature::update() {
  float value = this->parent_->get_temperature();
  this->publish_state(value);
}

}  // namespace esphome::seesaw
