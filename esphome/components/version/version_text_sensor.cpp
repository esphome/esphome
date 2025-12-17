#include "version_text_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"

namespace esphome {
namespace version {

static const char *const TAG = "version.text_sensor";

void VersionTextSensor::setup() {
  static const char PREFIX[] PROGMEM = ESPHOME_VERSION " (config hash 0x";
  static const char BUILT_STR[] PROGMEM = ", built ";
  // Buffer size: PREFIX + 8 hex chars + BUILT_STR + BUILD_TIME_STR_SIZE + ")" + null
  constexpr size_t buf_size = sizeof(PREFIX) + 8 + sizeof(BUILT_STR) + esphome::Application::BUILD_TIME_STR_SIZE + 2;
  char version_str[buf_size];

  ESPHOME_strncpy_P(version_str, PREFIX, sizeof(version_str));

  size_t len = strlen(version_str);
  snprintf(version_str + len, sizeof(version_str) - len, "%08" PRIx32, App.get_config_hash());

  if (!this->hide_timestamp_) {
    size_t len = strlen(version_str);
    ESPHOME_strncat_P(version_str, BUILT_STR, sizeof(version_str) - len - 1);
    ESPHOME_strncat_P(version_str, ESPHOME_BUILD_TIME_STR, sizeof(version_str) - strlen(version_str) - 1);
  }

  strncat(version_str, ")", sizeof(version_str) - strlen(version_str) - 1);
  version_str[sizeof(version_str) - 1] = '\0';
  this->publish_state(version_str);
}
float VersionTextSensor::get_setup_priority() const { return setup_priority::DATA; }
void VersionTextSensor::set_hide_timestamp(bool hide_timestamp) { this->hide_timestamp_ = hide_timestamp; }
void VersionTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Version Text Sensor", this); }

}  // namespace version
}  // namespace esphome
