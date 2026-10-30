#include "systa_bus_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome::systa_bus {

void SystaSolarAquaTextSensor::handle_message(std::span<const uint8_t> message) {
  if (get_message_type(message) == MESSAGE_TYPE_AQUA_SENSOR_DATA) {
    if (this->error_code_text_sensor_ != nullptr) {
      if (message[15] == 0) {
        this->error_code_text_sensor_->publish_state("--");
      } else {
        char buf[4];
        int len = snprintf(buf, sizeof(buf), "%02d", message[15]);
        this->error_code_text_sensor_->publish_state(buf, static_cast<size_t>(len));
      }
    }
  }
}

}  // namespace esphome::systa_bus
