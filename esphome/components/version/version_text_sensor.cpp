#include "version_text_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/version.h"

namespace esphome::version {

static const char *const TAG = "version.text_sensor";

void VersionTextSensor::setup() {
  static const char HASH_PREFIX[] PROGMEM = ESPHOME_VERSION " (config hash 0x";
  static const char VERSION_PREFIX[] PROGMEM = ESPHOME_VERSION;
  static const char BUILT_STR[] PROGMEM = ", built ";

  // Buffer size: HASH_PREFIX + 8 hex chars + BUILT_STR + BUILD_TIME_STR_SIZE + ")" + null
  constexpr size_t buf_size =
      sizeof(HASH_PREFIX) + 8 + sizeof(BUILT_STR) + esphome::Application::BUILD_TIME_STR_SIZE + 2;
  char version_str[buf_size];

  // hide_hash restores the pre-2026.1 base format by omitting
  // the " (config hash 0x...)" suffix entirely.
  if (this->hide_hash_) {
    ESPHOME_strncpy_P(version_str, VERSION_PREFIX, sizeof(version_str));
  } else {
    ESPHOME_strncpy_P(version_str, HASH_PREFIX, sizeof(version_str));

    size_t len = strlen(version_str);
    snprintf(version_str + len, sizeof(version_str) - len, "%08" PRIx32, App.get_config_hash());
  }

  // Keep hide_timestamp behavior independent from hide_hash so all
  // combinations remain available to users.
  if (!this->hide_timestamp_) {
    size_t len = strlen(version_str);
    ESPHOME_strncat_P(version_str, BUILT_STR, sizeof(version_str) - len - 1);
    char build_time_buf[Application::BUILD_TIME_STR_SIZE];
    App.get_build_time_string(build_time_buf);
    strncat(version_str, build_time_buf, sizeof(version_str) - strlen(version_str) - 1);
  }

  // The closing parenthesis is part of the config-hash suffix and must
  // only be appended when that suffix is present.
  if (!this->hide_hash_) {
    strncat(version_str, ")", sizeof(version_str) - strlen(version_str) - 1);
  }
  version_str[sizeof(version_str) - 1] = '\0';
  this->publish_state(version_str);
}
void VersionTextSensor::set_hide_hash(bool hide_hash) { this->hide_hash_ = hide_hash; }
void VersionTextSensor::set_hide_timestamp(bool hide_timestamp) { this->hide_timestamp_ = hide_timestamp; }
void VersionTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Version Text Sensor", this); }

}  // namespace esphome::version
