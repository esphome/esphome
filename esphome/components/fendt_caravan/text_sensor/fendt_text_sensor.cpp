#ifdef USE_ESP32

#include "fendt_text_sensor.h"

namespace esphome::fendt_caravan {

static const char *const TAG = "FC.TS";

void FendtTextSensor::on_decoded(const std::string &value) {
  ESP_LOGV(TAG, "Decoded data for:%s value:%s", this->key_name_.c_str(), value.c_str());
  this->publish_state(value);
}
}  // namespace esphome::fendt_caravan
#endif
