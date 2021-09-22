#ifdef USE_ARDUINO

#include "gps_time.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gps {

static const char *const TAG = "gps.time";

void GPSTime::from_tiny_gps_(TinyGPSPlus &tiny_gps) {
  if (!tiny_gps.time.isValid() || !tiny_gps.date.isValid())
    return;
  if (!tiny_gps.time.isUpdated() || !tiny_gps.date.isUpdated())
    return;
  if (tiny_gps.date.year() < 2019)
    return;

  time::ESPTime val{};
  val.year = tiny_gps.date.year();
  val.month = tiny_gps.date.month();
  val.day_of_month = tiny_gps.date.day();
  // Set these to valid value for  recalc_timestamp_utc - it's not used for calculation
  val.day_of_week = 1;
  val.day_of_year = 1;

  val.hour = tiny_gps.time.hour();
  val.minute = tiny_gps.time.minute();
  val.second = tiny_gps.time.second();
  val.recalc_timestamp_utc(false);
  this->synchronize_epoch_(val.timestamp);
  this->has_time_ = true;
}

}  // namespace gps
}  // namespace esphome

#endif  // USE_ARDUINO
