#include "geolocation_time.h"
#include "esphome/core/hal.h"

namespace esphome::geolocation {

void GeolocationTime::sync_(const GeolocationData &data) {
  const time_t utc = current_utc(data, millis_64());
  if (utc == 0)
    return;
  this->synchronize_epoch_(utc);
  this->has_time_ = true;
}

}  // namespace esphome::geolocation
