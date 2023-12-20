#include "text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2420 {

static const char *const TAG = "LD2420.text_sensor";

void LD2420TextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2420 TextSensor:");
  LOG_TEXT_SENSOR("  ", "Firmware", this->fw_version_text_sensor_);
}

}  // namespace ld2420
}  // namespace esphome
