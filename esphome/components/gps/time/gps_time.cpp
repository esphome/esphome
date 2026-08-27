#include "gps_time.h"
#include "esphome/core/log.h"

namespace esphome::gps {

static const char *const TAG = "gps.time";

void GPSTime::from_tiny_gps_(TinyGPSPlus &tiny_gps) {
  if (!tiny_gps.time.isValid() || !tiny_gps.date.isValid() || !tiny_gps.time.isUpdated() ||
      !tiny_gps.date.isUpdated() || tiny_gps.date.year() < 2025) {
    return;
  }

  ESPTime val{};
  val.year = tiny_gps.date.year();
  val.month = tiny_gps.date.month();
  val.day_of_month = tiny_gps.date.day();
  val.hour = tiny_gps.time.hour();
  val.minute = tiny_gps.time.minute();
  val.second = tiny_gps.time.second();
  val.recalc_timestamp_utc(false);
  this->synchronize_epoch_(val.timestamp);
  this->has_time_ = true;
}

}  // namespace esphome::gps
