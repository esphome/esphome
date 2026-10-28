#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

#include "esphome/components/time/real_time_clock.h"

namespace esphome::sun {

namespace internal {

/* Usually, ESPHome uses single-precision floating point values
 * because those tend to be accurate enough and are more efficient.
 *
 * However, some of the data in this class has to be quite accurate, so double is
 * used everywhere.
 */
using num_t = double;
struct GeoLocation {
  num_t latitude;
  num_t longitude;

  num_t latitude_rad() const;
  num_t longitude_rad() const;
};

struct Moment {
  ESPTime dt;

  num_t jd() const;
  num_t jde() const;
};

struct EquatorialCoordinate {
  num_t right_ascension;
  num_t declination;

  num_t right_ascension_rad() const;
  num_t declination_rad() const;
};

struct HorizontalCoordinate {
  num_t elevation;
  num_t azimuth;

  num_t elevation_rad() const;
  num_t azimuth_rad() const;
};

}  // namespace internal

class Sun final {
 public:
  void set_time(time::RealTimeClock *time) { time_ = time; }
  time::RealTimeClock *get_time() const { return time_; }
  void set_latitude(double latitude) { location_.latitude = latitude; }
  void set_longitude(double longitude) { location_.longitude = longitude; }

  // Check if the sun is above the horizon, with a default elevation angle of -0.83333 (standard for sunrise/set).
  bool is_above_horizon(double elevation = -0.83333) { return this->elevation() > elevation; }

  optional<ESPTime> sunrise(double elevation);
  optional<ESPTime> sunset(double elevation);
  optional<ESPTime> sunrise(ESPTime date, double elevation);
  optional<ESPTime> sunset(ESPTime date, double elevation);

  double elevation();
  double azimuth();

 protected:
  internal::HorizontalCoordinate calc_coords_();
  optional<ESPTime> calc_event_(bool rising, double zenith);
  optional<ESPTime> calc_event_(ESPTime date, bool rising, double zenith);

  time::RealTimeClock *time_;
  internal::GeoLocation location_;
};

/// on_sunrise filter: the elevation was at or below `elevation` on the previous poll and is above it now.
inline bool rose_past(double last, double current, double elevation) {
  return last <= elevation && elevation < current;
}
/// on_sunset filter: the elevation was at or above `elevation` on the previous poll and is below it now.
inline bool set_past(double last, double current, double elevation) { return last >= elevation && elevation > current; }

#ifdef SUN_ELEVATION_CALLBACK_COUNT
/// Polls the elevation once a minute for all on_sunrise/on_sunset automations.
class SunElevationPoller final : public PollingComponent, public Parented<Sun> {
 public:
  explicit SunElevationPoller(Sun *parent) : PollingComponent(60000), Parented<Sun>(parent) {}

  /// Called with the previous and the current elevation, from the second valid poll on.
  template<typename F> void add_on_elevation_callback(F &&callback) {
    this->elevation_callback_.add(std::forward<F>(callback));
  }

  void update() override;

 protected:
  double last_elevation_{NAN};
  StaticCallbackManager<SUN_ELEVATION_CALLBACK_COUNT, void(double, double)> elevation_callback_;
};
#endif

}  // namespace esphome::sun
