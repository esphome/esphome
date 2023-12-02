#include "copy_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.text_sensor";

void CopyTextSensor::setup() {
  source_->add_on_state_callback([this](const std::string &value) { this->publish_state(value); });
  if (source_->has_state())
    this->publish_state(source_->state);
}

void CopyTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Copy Sensor", this); }

}  // namespace copy
}  // namespace esphome
