#include "gps_time.h"

namespace esphome::gps {

void GPSTime::on_update(const GPSData &data) {
  if (!this->sync_pending_ || !data.is_updated(GPS_FIELD_TIME) || !data.is_updated(GPS_FIELD_DATE) ||
      data.year < 2025) {
    return;
  }

  ESPTime val{};
  val.year = data.year;
  val.month = data.month;
  val.day_of_month = data.day;
  val.hour = data.hour;
  val.minute = data.minute;
  val.second = data.second;
  val.recalc_timestamp_utc(false);
  this->sync_pending_ = false;
  this->synchronize_epoch_(val.timestamp);
}

}  // namespace esphome::gps
