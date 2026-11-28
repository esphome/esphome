#include "gps.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::gps {

ESPHOME_LOG_TAG(TAG, "gps");

const GPSData &GPSListener::get_data() const { return this->parent_->get_data(); }

#ifdef USE_GPS_FIELD_AGE
uint32_t GPSListener::get_age(GPSField field) const { return this->parent_->get_age(field); }

uint32_t GPS::get_age(GPSField field) const {
  if (!this->data_.has(field))
    return UINT32_MAX;
  return millis() - this->updated_ms_[__builtin_ctz(field)];
}
#endif

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
#ifdef USE_GPS_FIELD_AGE
  // Hold old timestamps at 2^31 ms in the past so millis() rollover can't make a stale field look fresh.
  const uint32_t now = millis();
  for (auto &ms : this->updated_ms_) {
    if (now - ms > AGE_LIMIT_MS)
      ms = now - AGE_LIMIT_MS;
  }
#endif
  const GPSData &d = this->data_;
  PUBLISH_STATE(this->latitude_sensor_, d.has(GPS_FIELD_LOCATION) ? d.latitude() : NAN);
  PUBLISH_STATE(this->longitude_sensor_, d.has(GPS_FIELD_LOCATION) ? d.longitude() : NAN);
  PUBLISH_STATE(this->speed_sensor_, d.has(GPS_FIELD_SPEED) ? d.speed_kmh() : NAN);
  PUBLISH_STATE(this->course_sensor_, d.has(GPS_FIELD_COURSE) ? d.course() : NAN);
  PUBLISH_STATE(this->altitude_sensor_, d.has(GPS_FIELD_ALTITUDE) ? d.altitude() : NAN);
  PUBLISH_STATE(this->satellites_sensor_, d.has(GPS_FIELD_SATELLITES) ? d.satellites : NAN);
  PUBLISH_STATE(this->hdop_sensor_, d.has(GPS_FIELD_HDOP) ? d.hdop() : NAN);
}

void GPS::loop() {
  this->read_available([this](uint8_t c) {
    size_t len;
    if (char *line = this->line_reader_.feed(c, len)) {
      this->handle_line_(line, len);
    } else if (this->line_reader_.overflowed()) {
      ESP_LOGV(TAG, "Line over %u characters dropped", static_cast<unsigned>(NMEA_MAX_LINE));
    }
  });
}

void GPS::handle_line_(char *line, size_t len) {
  if (!parse_nmea_sentence(line, len, this->data_))
    return;
  const GPSData &d = this->data_;
#ifdef USE_GPS_FIELD_AGE
  const uint32_t now = App.get_loop_component_start_time();
  for (uint8_t i = 0; i != GPS_FIELD_COUNT; i++) {
    if (d.updated & (1 << i))
      this->updated_ms_[i] = now;
  }
#endif
  if (d.is_updated(GPS_FIELD_LOCATION)) {
    ESP_LOGV(TAG, "Latitude, Longitude: %.7f, %.7f", d.latitude_e7 * 1e-7, d.longitude_e7 * 1e-7);
  }
  for (auto *listener : this->listeners_) {
    listener->on_update(d);
  }
}

}  // namespace esphome::gps
