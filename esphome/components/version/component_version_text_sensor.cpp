#include "component_version_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome::version {

static const char *const TAG = "version.text_sensor";

void ComponentVersionTextSensor::setup() {
  // Publish nothing when the component declares no version, so the state reads as
  // unknown rather than as a placeholder that cannot be told apart from a real value.
  if (this->version_ != nullptr) {
    this->publish_state(this->version_);
  }
}

void ComponentVersionTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Component Version Text Sensor", this);
  ESP_LOGCONFIG(TAG,
                "  Component: %s\n"
                "  Version: %s",
                this->component_name_, this->version_ != nullptr ? this->version_ : "unknown (not reported)");
}

}  // namespace esphome::version
