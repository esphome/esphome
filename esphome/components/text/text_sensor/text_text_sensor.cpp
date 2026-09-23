#include "text_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome::text {

static const char *const TAG = "text.text_sensor";

void TextTextSensor::setup() {
  this->source_->add_on_state_callback([this](const std::string &value) { this->publish_state(value); });
  if (this->source_->has_state())
    this->publish_state(this->source_->state);
}

void TextTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Text Text Sensor", this); }

}  // namespace esphome::text
