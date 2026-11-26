#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <cmath>
#include <ctime>

namespace esphome::gnss {

/// The latest solution of a receiver. Unset values are NAN (0 for satellites and utc).
struct GNSSData {
  double latitude{NAN};    // degrees
  double longitude{NAN};   // degrees
  uint64_t utc_millis{0};  // millis_64() when utc was received
  time_t utc{0};           // UTC of the solution
  float altitude{NAN};     // m above mean sea level
  float speed{NAN};        // km/h
  float course{NAN};       // degrees from true north
  float hdop{NAN};
  uint8_t satellites{0};
};

/// Receivers send UTC at least once a second; an older one means the receiver stopped sending it.
static constexpr uint64_t MAX_UTC_AGE_MS = 10000;

/// The UTC of `data` advanced to `now_ms` (a millis_64() value), or 0 when it has none or it is stale.
inline time_t current_utc(const GNSSData &data, uint64_t now_ms) {
  if (data.utc == 0)
    return 0;
  const uint64_t age = now_ms - data.utc_millis;
  if (age > MAX_UTC_AGE_MS)
    return 0;
  return data.utc + static_cast<time_t>(age / 1000);
}

/// Base of the gnss platforms: they fill data_ and call publish_fix_() after each parsed message.
class GNSSComponent : public PollingComponent {
 public:
  void update() override {
#ifdef USE_GNSS_DATA_PUBLISHER
    if (this->data_publisher_ != nullptr)
      this->data_publisher_(this->data_);
#endif
  }

  const GNSSData &get_data() const { return this->data_; }

#ifdef USE_GNSS_DATA_PUBLISHER
  HUB_PUBLISHER(data, const GNSSData &)
#endif
#ifdef USE_GNSS_FIX_PUBLISHER
  HUB_PUBLISHER(fix, const GNSSData &)
#endif

 protected:
  void publish_fix_() {
#ifdef USE_GNSS_FIX_PUBLISHER
    if (this->fix_publisher_ != nullptr)
      this->fix_publisher_(this->data_);
#endif
  }

  GNSSData data_{};
};

}  // namespace esphome::gnss
