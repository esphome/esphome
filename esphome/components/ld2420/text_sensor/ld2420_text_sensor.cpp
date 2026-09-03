#include "ld2420_text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ld2420 {

static const char *const TAG = "ld2420.text_sensor";

void LD2420TextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Text Sensor:");
  LOG_TEXT_SENSOR("  ", "Firmware", this->fw_version_text_sensor_);
}

}  // namespace esphome::ld2420
