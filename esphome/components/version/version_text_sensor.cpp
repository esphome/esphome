#include "version_text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace version {

static const char *const TAG = "version.text_sensor";

void VersionTextSensor::setup() {
  if (this->hide_timestamp_) {
    this->publish_state(ESPHOME_VERSION);
  } else {
    this->publish_state(str_sprintf(ESPHOME_VERSION " %s", App.get_compilation_time_ref().c_str()));
  }
}
float VersionTextSensor::get_setup_priority() const { return setup_priority::DATA; }
void VersionTextSensor::set_hide_timestamp(bool hide_timestamp) { this->hide_timestamp_ = hide_timestamp; }
void VersionTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Version Text Sensor", this); }

}  // namespace version
}  // namespace esphome
