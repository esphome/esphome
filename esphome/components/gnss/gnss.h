#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <cstdint>
#include <ctime>

namespace esphome::gnss {

/// Bits of GNSSData::updated and GNSSData::valid.
enum GNSSField : uint16_t {
  GNSS_FIELD_LOCATION = 1 << 0,
  GNSS_FIELD_SPEED = 1 << 1,
  GNSS_FIELD_COURSE = 1 << 2,
  GNSS_FIELD_ALTITUDE = 1 << 3,
  GNSS_FIELD_SATELLITES = 1 << 4,
  GNSS_FIELD_HDOP = 1 << 5,
  GNSS_FIELD_TIME = 1 << 6,
  GNSS_FIELD_DATE = 1 << 7,
  GNSS_FIELD_HORIZONTAL_ACCURACY = 1 << 8,
  GNSS_FIELD_VERTICAL_ACCURACY = 1 << 9,
};

/// The latest solution of a receiver as scaled integers; a name ending in eN is the value times 10^N. A value is
/// set once has() gives true for its field.
struct GNSSData {
  uint64_t utc_millis{0};              ///< millis_64() when utc was received
  time_t utc{0};                       ///< UTC of the solution, 0 when there is none
  int32_t latitude_e7{0};              ///< degrees, north positive
  int32_t longitude_e7{0};             ///< degrees, east positive
  int32_t altitude_e3{0};              ///< meters above mean sea level
  uint32_t speed_e3{0};                ///< knots
  uint32_t horizontal_accuracy_e3{0};  ///< meters, estimated
  uint32_t vertical_accuracy_e3{0};    ///< meters, estimated
  uint16_t course_e2{0};               ///< degrees from true north
  uint16_t hdop_e2{0};
  uint16_t year{0};
  uint8_t month{0};
  uint8_t day{0};
  uint8_t hour{0};  ///< UTC
  uint8_t minute{0};
  uint8_t second{0};
  uint8_t satellites{0};
  uint16_t updated{0};  ///< GNSSField bits set by the last message
  uint16_t valid{0};    ///< GNSSField bits set by any message so far

  bool has(GNSSField field) const { return (this->valid & field) != 0; }
  bool is_updated(GNSSField field) const { return (this->updated & field) != 0; }

  // A double quotient rounds once to the float nearest the decimal value
  float latitude() const { return static_cast<float>(this->latitude_e7 / 1e7); }
  float longitude() const { return static_cast<float>(this->longitude_e7 / 1e7); }
  float altitude() const { return static_cast<float>(this->altitude_e3 / 1e3); }
  float speed_knots() const { return static_cast<float>(this->speed_e3 / 1e3); }
  float speed_kmh() const { return static_cast<float>(this->speed_e3 * 1852.0 / 1e6); }
  float course() const { return static_cast<float>(this->course_e2 / 1e2); }
  float hdop() const { return static_cast<float>(this->hdop_e2 / 1e2); }
  float horizontal_accuracy() const { return static_cast<float>(this->horizontal_accuracy_e3 / 1e3); }
  float vertical_accuracy() const { return static_cast<float>(this->vertical_accuracy_e3 / 1e3); }
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
