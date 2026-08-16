#include "ld2410s.h"
namespace esphome::ld2410s {
void LD2410S::publish_distance_(uint16_t distance, bool force_publish) {
#ifdef USE_SENSOR
  if (this->distance_sensor_ != nullptr) {
    if (this->distance_sensor_->state != distance || force_publish) {
      this->distance_sensor_->publish_state(distance);
    }
  }
#endif
}
void LD2410S::publish_presence_(bool presence, bool force_publish) {
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    if (this->presence_binary_sensor_->state != presence || force_publish) {
      this->presence_binary_sensor_->publish_state(presence);
    }
  }
#endif
}
}  // namespace esphome::ld2410s
