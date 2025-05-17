#ifdef USE_ARDUINO

#include "gps.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gps {

static const char *const TAG = "gps";

TinyGPSPlus &GPSListener::get_tiny_gps() { return this->parent_->get_tiny_gps(); }

void GPS::dump_config() {
  ESP_LOGCONFIG(TAG, "GPS:");
  LOG_SENSOR("  ", "Latitude", this->latitude_sensor_);
  LOG_SENSOR("  ", "Longitude", this->longitude_sensor_);
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
  LOG_SENSOR("  ", "Course", this->course_sensor_);
  LOG_SENSOR("  ", "Altitude", this->altitude_sensor_);
  LOG_SENSOR("  ", "Satellites", this->satellites_sensor_);
  LOG_SENSOR("  ", "HDOP", this->hdop_sensor_);
}

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

  if (this->hdop_sensor_ != nullptr)
    this->hdop_sensor_->publish_state(this->hdop_);
}

void GPS::loop() {
  while (this->available() > 0 && !this->has_time_) {
    if (this->tiny_gps_.encode(this->read())) {
      if (this->tiny_gps_.location.isUpdated()) {
        this->latitude_ = this->tiny_gps_.location.lat();
        this->longitude_ = this->tiny_gps_.location.lng();

        ESP_LOGD(TAG, "Location:");
        ESP_LOGD(TAG, "  Lat: %.6f °", this->latitude_);
        ESP_LOGD(TAG, "  Lon: %.6f °", this->longitude_);
      }

      if (this->tiny_gps_.speed.isUpdated()) {
        this->speed_ = this->tiny_gps_.speed.kmph();
        ESP_LOGD(TAG, "Speed: %.3f km/h", this->speed_);
      }

      if (this->tiny_gps_.course.isUpdated()) {
        this->course_ = this->tiny_gps_.course.deg();
        ESP_LOGD(TAG, "Course: %.2f °", this->course_);
      }

      if (this->tiny_gps_.altitude.isUpdated()) {
        this->altitude_ = this->tiny_gps_.altitude.meters();
        ESP_LOGD(TAG, "Altitude: %.2f m", this->altitude_);
      }

      if (this->tiny_gps_.satellites.isUpdated()) {
        this->satellites_ = this->tiny_gps_.satellites.value();
        ESP_LOGD(TAG, "Satellites: %d", this->satellites_);
      }

      if (this->tiny_gps_.hdop.isUpdated()) {
        this->hdop_ = this->tiny_gps_.hdop.hdop();
        ESP_LOGD(TAG, "HDOP: %.3f", this->hdop_);
      }

      for (auto *listener : this->listeners_) {
        listener->on_update(this->tiny_gps_);
      }
    }
  }
}

}  // namespace gps
}  // namespace esphome

#endif  // USE_ARDUINO
