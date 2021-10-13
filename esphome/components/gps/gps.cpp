#ifdef USE_ARDUINO

#include "gps.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gps {

static const char *const TAG = "gps";

TinyGPSPlus &GPSListener::get_tiny_gps() { return this->parent_->get_tiny_gps(); }

void GPS::update() {
  if (this->latitude_sensor_ != nullptr)
    this->latitude_sensor_->publish_state(this->latitude_);

  if (this->longitude_sensor_ != nullptr)
    this->longitude_sensor_->publish_state(this->longitude_);

  if (this->speed_sensor_ != nullptr)
    this->speed_sensor_->publish_state(this->speed_);

  if (this->course_sensor_ != nullptr)
    this->course_sensor_->publish_state(this->course_);

  if (this->altitude_sensor_ != nullptr)
    this->altitude_sensor_->publish_state(this->altitude_);

  if (this->satellites_sensor_ != nullptr)
    this->satellites_sensor_->publish_state(this->satellites_);
}

void GPS::loop() {
  while (this->available() && !this->has_time_) {
    if (this->tiny_gps_.encode(this->read())) {
      if (tiny_gps_.location.isUpdated()) {
        this->latitude_ = tiny_gps_.location.lat();
        this->longitude_ = tiny_gps_.location.lng();

        ESP_LOGD(TAG, "Location:");
        ESP_LOGD(TAG, "  Lat: %f", this->latitude_);
        ESP_LOGD(TAG, "  Lon: %f", this->longitude_);
      }

      if (tiny_gps_.speed.isUpdated()) {
        this->speed_ = tiny_gps_.speed.kmph();
        ESP_LOGD(TAG, "Speed:");
        ESP_LOGD(TAG, "  %f km/h", this->speed_);
      }
      if (tiny_gps_.course.isUpdated()) {
        this->course_ = tiny_gps_.course.deg();
        ESP_LOGD(TAG, "Course:");
        ESP_LOGD(TAG, "  %f °", this->course_);
      }
      if (tiny_gps_.altitude.isUpdated()) {
        this->altitude_ = tiny_gps_.altitude.meters();
        ESP_LOGD(TAG, "Altitude:");
        ESP_LOGD(TAG, "  %f m", this->altitude_);
      }
      if (tiny_gps_.satellites.isUpdated()) {
        this->satellites_ = tiny_gps_.satellites.value();
        ESP_LOGD(TAG, "Satellites:");
        ESP_LOGD(TAG, "  %d", this->satellites_);
      }

      for (auto *listener : this->listeners_)
        listener->on_update(this->tiny_gps_);
    }
  }
}

}  // namespace gps
}  // namespace esphome

#endif  // USE_ARDUINO
