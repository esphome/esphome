#include "version_text_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace version {

static const char *const TAG = "version.text_sensor";

void VersionTextSensor::setup() {
  char version_str[128];
  if (this->hide_timestamp_) {
    snprintf_P(version_str, sizeof(version_str), PSTR(ESPHOME_VERSION " (%08" PRIx32 ")"), App.get_config_hash());
  } else {
    char build_time_str[esphome::Application::BUILD_TIME_STR_SIZE];
    App.get_build_time_string(build_time_str);
    snprintf_P(version_str, sizeof(version_str), PSTR(ESPHOME_VERSION " (%08" PRIx32 ", built: %s)"),
               App.get_config_hash(), build_time_str);
  }
  this->publish_state(version_str);
}
float VersionTextSensor::get_setup_priority() const { return setup_priority::DATA; }
void VersionTextSensor::set_hide_timestamp(bool hide_timestamp) { this->hide_timestamp_ = hide_timestamp; }
void VersionTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Version Text Sensor", this); }

}  // namespace version
}  // namespace esphome
