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
    size_t len;
    if (char *line = this->line_reader_.feed(c, len)) {
      this->handle_line_(line, len);
    } else if (this->line_reader_.overflowed()) {
      ESP_LOGV(TAG, "Line over %u characters dropped", static_cast<unsigned>(NMEA_MAX_LINE));
    }
  });
}

void NMEAGNSS::handle_line_(char *line, size_t len) {
  auto &d = this->data_;
  if (!parse_nmea_sentence(line, len, d))
    return;
  if (d.is_updated(geolocation::GEOLOCATION_FIELD_LOCATION)) {
    ESP_LOGV(TAG, "Latitude, Longitude: %.7f°, %.7f°", d.latitude_e7 * 1e-7, d.longitude_e7 * 1e-7);
  }
  // The parser takes the time and date only from an RMC with a fix. A receiver that lost track of a GPS week
  // rollover sends a date from before its firmware was built.
  if (d.is_updated(geolocation::GEOLOCATION_FIELD_TIME) && d.is_updated(geolocation::GEOLOCATION_FIELD_DATE) &&
      d.year >= 2025) {
    ESPTime t{};
    t.year = d.year;
    t.month = d.month;
    t.day_of_month = d.day;
    t.hour = d.hour;
    t.minute = d.minute;
    t.second = d.second;
    t.recalc_timestamp_utc(false);
    d.utc = t.timestamp;
    d.utc_millis = millis_64();
  }
  this->publish_fix_();
}

}  // namespace esphome::nmea
