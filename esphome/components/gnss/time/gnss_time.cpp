#include "gnss_time.h"
#include "esphome/core/hal.h"

namespace esphome::gnss {

void GNSSTime::sync_(const GNSSData &data) {
  if (data.utc == 0)
    return;
  // The solution may be a few seconds old by the time this runs
  this->synchronize_epoch_(data.utc + (millis() - data.utc_millis) / 1000);
  this->has_time_ = true;
}

}  // namespace esphome::gnss
