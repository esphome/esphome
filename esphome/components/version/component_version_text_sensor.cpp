#include "component_version_text_sensor.h"

#include <cstring>

namespace esphome::version {

static const char *const TAG = "version.text_sensor";

std::string ComponentVersionTextSensor::version_string_() const {
#ifdef USE_ESP8266
  // PROGMEM pointer, so it must be read with the _P functions. Exact-size, as in
  // TemplatableStringValue.
  auto *version = reinterpret_cast<ESPHOME_PGM_P>(this->version_);
  size_t len = strlen_P(version);
  std::string result(len, '\0');
  memcpy_P(result.data(), version, len);
  return result;
#else
  return this->version_;
#endif
}

void ComponentVersionTextSensor::setup() {
  // Publish nothing when the component declares no version, so the state reads as
  // unknown rather than as a placeholder that cannot be told apart from a real value.
  if (this->version_ != nullptr) {
    this->publish_state(this->version_string_());
  }
}

void ComponentVersionTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Component Version Text Sensor", this);
  const std::string version = this->version_ != nullptr ? this->version_string_() : "unknown (not reported)";
  ESP_LOGCONFIG(TAG,
                "  Component: %s\n"
                "  Version: %s",
                LOG_STR_ARG(this->component_name_), version.c_str());
}

}  // namespace esphome::version
