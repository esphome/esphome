#include "version_text_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace version {

static const char *const TAG = "version.text_sensor";

void VersionTextSensor::setup() {
  static const char PREFIX[] PROGMEM = ESPHOME_VERSION " (config hash 0x";
  char version_str[128];

#ifdef USE_ESP8266
  strcpy_P(version_str, PREFIX);
#else
  strcpy(version_str, PREFIX);
#endif

  char hash_str[9];
  snprintf(hash_str, sizeof(hash_str), "%08" PRIx32, App.get_config_hash());
  strcat(version_str, hash_str);

  if (!this->hide_timestamp_) {
    strcat(version_str, ", built: ");
    char build_time_str[esphome::Application::BUILD_TIME_STR_SIZE];
    App.get_build_time_string(build_time_str);
    strcat(version_str, build_time_str);
  }

  strcat(version_str, ")");
  this->publish_state(version_str);
}
float VersionTextSensor::get_setup_priority() const { return setup_priority::DATA; }
void VersionTextSensor::set_hide_timestamp(bool hide_timestamp) { this->hide_timestamp_ = hide_timestamp; }
void VersionTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Version Text Sensor", this); }

}  // namespace version
}  // namespace esphome
