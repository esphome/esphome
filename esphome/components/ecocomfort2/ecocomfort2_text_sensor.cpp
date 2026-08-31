#ifdef USE_ESP32

#include "ecocomfort2_text_sensor.h"
#include "esphome/core/log.h"

#include <cstdio>
#include <cstring>

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.text_sensor";

void Ecocomfort2TextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Text Sensor:");
  LOG_TEXT_SENSOR("  ", "Firmware", this->firmware_sensor_);
}

void Ecocomfort2TextSensor::on_status() {
  if (this->firmware_sensor_ != nullptr) {
    const char *fw = this->parent_->get_firmware_version();
    if (fw[0] != '\0' && std::strcmp(fw, this->last_firmware_) != 0) {
      snprintf(this->last_firmware_, sizeof(this->last_firmware_), "%s", fw);
      this->firmware_sensor_->publish_state(fw);
    }
  }
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif
