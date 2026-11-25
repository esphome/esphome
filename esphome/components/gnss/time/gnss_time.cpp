#include "gnss_time.h"
#include "esphome/core/hal.h"

namespace esphome::gnss {

// Receivers send UTC at least once a second; an older one means the receiver stopped sending it
static constexpr uint64_t MAX_UTC_AGE_MS = 10000;

void GNSSTime::sync_(const GNSSData &data) {
  if (data.utc == 0)
    return;
  const uint64_t age = millis_64() - data.utc_millis;
  if (age > MAX_UTC_AGE_MS)
    return;
  this->synchronize_epoch_(data.utc + age / 1000);
  this->has_time_ = true;
}

}  // namespace esphome::gnss
