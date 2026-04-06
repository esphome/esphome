#include "systa_bus_text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::systa_bus {

static const char *const TAG = "systa_bus.text_sensor";

void SystaSolarAquaTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SystaSolar Aqua:");
  LOG_TEXT_SENSOR("  ", "Error code", this->error_code_text_sensor_);
}

void SystaSolarAquaTextSensor::handle_message(std::vector<uint8_t> &message) {
  if (get_message_type(message) == MESSAGE_TYPE_AQUA_SENSOR_DATA) {
    if (this->error_code_text_sensor_ != nullptr) {
      char buf[4];
      int len = snprintf(buf, sizeof(buf), message[15] == 0 ? "--" : "%02d", message[15]);
      this->error_code_text_sensor_->publish_state(buf, static_cast<size_t>(len));
    }
  }
}

}  // namespace esphome::systa_bus
