#ifdef USE_ESP32
#include "fendt_sensor.h"

namespace esphome::fendt_caravan {
static const char *const TAG = "FC.FS";

void FendtSensor::on_decoded(const float &value) {
  ESP_LOGV(TAG, "Decoded data for:%s value:%f", this->key_name_.c_str(), value);
  this->publish_state(value);
}
}  // namespace esphome::fendt_caravan
#endif
