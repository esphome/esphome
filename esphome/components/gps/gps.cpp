#include "gps.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gps {

static const char *TAG = "gps";

TinyGPSPlus &GPSListener::get_tiny_gps() { return this->parent_->get_tiny_gps(); }

void GPS::update() {

  if (this->latitude_sensor_ != nullptr)
    this->latitude_sensor_->publish_state(this->latitude);

  if (this->longitude_sensor_ != nullptr)
    this->longitude_sensor_->publish_state(this->longitude);
  
  if (this->speed_sensor_ != nullptr)
    this->speed_sensor_->publish_state(this->speed);

  if (this->course_sensor_ != nullptr)
    this->course_sensor_->publish_state(this->course);

  if (this->altitude_sensor_ != nullptr)
    this->altitude_sensor_->publish_state(this->altitude);

  if (this->satellites_sensor_ != nullptr)
    this->satellites_sensor_->publish_state(this->satellites);
  
}


void GPS::loop() {
  float lat, lng;

  while (this->available() && !this->has_time_) {
    if (this->tiny_gps_.encode(this->read())) {
      if (tiny_gps_.location.isUpdated()) {
        
        this->latitude = tiny_gps_.location.lat();
        this->longitude = tiny_gps_.location.lng();

        ESP_LOGD(TAG, "Location:");
        ESP_LOGD(TAG, "  Lat: %f", this->latitude);
        ESP_LOGD(TAG, "  Lon: %f", this->longitude);

      }

      if (tiny_gps_.speed.isUpdated()) {
        this->speed = tiny_gps_.speed.kmph();
        ESP_LOGD(TAG, "Speed:");
        ESP_LOGD(TAG, "  %f km/h", this->speed);
      }
      if (tiny_gps_.course.isUpdated()) {
        this->course = tiny_gps_.course.deg();
        ESP_LOGD(TAG, "Course:");
        ESP_LOGD(TAG, "  %f °", this->course);
      }
      if (tiny_gps_.altitude.isUpdated()) {
        this->altitude = tiny_gps_.altitude.meters();
        ESP_LOGD(TAG, "Altitude:");
        ESP_LOGD(TAG, "  %f m", this->altitude);
      }
      if (tiny_gps_.satellites.isUpdated()) {
        this->satellites = tiny_gps_.satellites.value();
        ESP_LOGD(TAG, "Satellites:");
        ESP_LOGD(TAG, "  %d", this->satellites);
      }

      for (auto *listener : this->listeners_)
        listener->on_update(this->tiny_gps_);
    }
  }
}

}  // namespace gps
}  // namespace esphome
