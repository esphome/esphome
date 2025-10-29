#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace sun {

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

class Sun {
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

class SunTrigger : public Trigger<>, public PollingComponent, public Parented<Sun> {
 public:
  SunTrigger() : PollingComponent(60000) {}

  void set_sunrise(bool sunrise) { sunrise_ = sunrise; }
  void set_elevation(double elevation) { elevation_ = elevation; }

  void update() override {
    double current = this->parent_->elevation();
    if (std::isnan(current))
      return;

    bool crossed;
    if (this->sunrise_) {
      crossed = this->last_elevation_ <= this->elevation_ && this->elevation_ < current;
    } else {
      crossed = this->last_elevation_ >= this->elevation_ && this->elevation_ > current;
    }

    if (crossed && !std::isnan(this->last_elevation_)) {
      this->trigger();
    }
    this->last_elevation_ = current;
  }

 protected:
  bool sunrise_;
  double last_elevation_{NAN};
  double elevation_;
};

template<typename... Ts> class SunCondition : public Condition<Ts...>, public Parented<Sun> {
 public:
  TEMPLATABLE_VALUE(double, elevation);
  void set_above(bool above) { above_ = above; }

  bool check(Ts... x) override {
    double elevation = this->elevation_.value(x...);
    double current = this->parent_->elevation();
    if (this->above_) {
      return current > elevation;
    } else {
      return current < elevation;
    }
  }

 protected:
  bool above_;
};

}  // namespace sun
}  // namespace esphome
