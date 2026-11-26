#include "nmea_gnss.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"

namespace esphome::nmea {

ESPHOME_LOG_TAG(TAG, "nmea");

void NMEAGNSS::dump_config() {
  ESP_LOGCONFIG(TAG, "NMEA GNSS:");
  LOG_UPDATE_INTERVAL(this);
}

void NMEAGNSS::loop() {
  this->read_available([this](uint8_t c) {
    if (this->tiny_gps_.encode(c))
      this->handle_message_();
  });
}

void NMEAGNSS::handle_message_() {
  auto &d = this->data_;
  auto &gps = this->tiny_gps_;
  if (gps.location.isUpdated()) {
    d.latitude = gps.location.lat();
    d.longitude = gps.location.lng();
    ESP_LOGV(TAG, "Latitude, Longitude: %.6f°, %.6f°", d.latitude, d.longitude);
  }
  if (gps.speed.isUpdated()) {
    d.speed = gps.speed.kmph();
    ESP_LOGV(TAG, "Speed: %.3f km/h", d.speed);
  }
  if (gps.course.isUpdated()) {
    d.course = gps.course.deg();
    ESP_LOGV(TAG, "Course: %.2f°", d.course);
  }
  if (gps.altitude.isUpdated()) {
    d.altitude = gps.altitude.meters();
    ESP_LOGV(TAG, "Altitude: %.2f m", d.altitude);
  }
  if (gps.satellites.isUpdated()) {
    d.satellites = gps.satellites.value();
    ESP_LOGV(TAG, "Satellites: %u", d.satellites);
  }
  if (gps.hdop.isUpdated()) {
    d.hdop = gps.hdop.hdop();
    ESP_LOGV(TAG, "HDOP: %.3f", d.hdop);
  }
  // A receiver without a fix sends its RTC date, which starts at its firmware's epoch
  if (gps.time.isUpdated() && gps.date.isUpdated() && gps.time.isValid() && gps.date.isValid() &&
      gps.date.year() >= 2025) {
    ESPTime t{};
    t.year = gps.date.year();
    t.month = gps.date.month();
    t.day_of_month = gps.date.day();
    t.hour = gps.time.hour();
    t.minute = gps.time.minute();
    t.second = gps.time.second();
    // TinyGPS++ commits the last date and time again for an RMC with empty fields, so only a new
    // UTC counts as received
    if (t.fields_in_range(false, false)) {
      t.recalc_timestamp_utc(false);
      if (t.timestamp != d.utc) {
        d.utc = t.timestamp;
        d.utc_millis = millis_64();
      }
    }
  }
  this->publish_fix_();
}

}  // namespace esphome::nmea
