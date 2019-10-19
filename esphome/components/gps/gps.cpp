#include "gps.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gps {

static const char *TAG = "gps";

TinyGPSPlus &GPSListener::get_tiny_gps() { return this->parent_->get_tiny_gps(); }

void GPS::loop() {
  while (this->available() && !this->has_time_) {
    if (this->tiny_gps_.encode(this->read())) {
      if (tiny_gps_.location.isUpdated()) {
        ESP_LOGD(TAG, "Location:");
        ESP_LOGD(TAG, "  Lat: %f", tiny_gps_.location.lat());
        ESP_LOGD(TAG, "  Lon: %f", tiny_gps_.location.lng());
      }

      if (tiny_gps_.speed.isUpdated()) {
        ESP_LOGD(TAG, "Speed:");
        ESP_LOGD(TAG, "  %f km/h", tiny_gps_.speed.kmph());
      }
      if (tiny_gps_.course.isUpdated()) {
        ESP_LOGD(TAG, "Course:");
        ESP_LOGD(TAG, "  %f °", tiny_gps_.course.deg());
      }
      if (tiny_gps_.altitude.isUpdated()) {
        ESP_LOGD(TAG, "Altitude:");
        ESP_LOGD(TAG, "  %f m", tiny_gps_.altitude.meters());
      }
      if (tiny_gps_.satellites.isUpdated()) {
        ESP_LOGD(TAG, "Satellites:");
        ESP_LOGD(TAG, "  %d", tiny_gps_.satellites.value());
      }
      if (tiny_gps_.satellites.isUpdated()) {
        ESP_LOGD(TAG, "HDOP:");
        ESP_LOGD(TAG, "  %.2f", tiny_gps_.hdop.hdop());
      }

      for (auto *listener : this->listeners_)
        listener->on_update(this->tiny_gps_);
    }
  }
}

}  // namespace gps
}  // namespace esphome
