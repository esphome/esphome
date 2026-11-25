#include "gnss_time.h"
#include "esphome/core/hal.h"

namespace esphome::gnss {

void GNSSTime::sync_(const GNSSData &data) {
  const time_t utc = current_utc(data, millis_64());
  if (utc == 0)
    return;
  this->synchronize_epoch_(utc);
  this->has_time_ = true;
}

}  // namespace esphome::gnss
