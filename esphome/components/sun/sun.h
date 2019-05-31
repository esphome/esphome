#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace sun {

class Sun {
 public:
  void set_time(time::RealTimeClock *time) { time_ = time; }
  time::RealTimeClock *get_time() const { return time_; }
  void set_latitude(double latitude) { latitude_ = latitude; }
  void set_longitude(double longitude) { longitude_ = longitude; }

  optional<time::ESPTime> sunrise(double elevation = 0.0);
  optional<time::ESPTime> sunset(double elevation = 0.0);

  double elevation();
  double azimuth();

 protected:
  double current_sun_time_() { return this->calc_sun_time_(this->time_->utcnow()); }

  /** Calculate the declination of the sun in rad.
   *
   * See https://en.wikipedia.org/wiki/Position_of_the_Sun#Declination_of_the_Sun_as_seen_from_Earth
   *
   * Accuracy: ±0.2°
   *
   * @param sun_time The day of the year, 1 means January 1st. See calc_sun_time_.
   * @return Sun declination in degrees
   */
  double sun_declination_(double sun_time);

  double elevation_ratio_(double sun_time);

  /** Calculate the hour angle based on the sun time of day in hours.
   *
   * Positive in morning, 0 at noon, negative in afternoon.
   *
   * @param sun_time Sun time, see calc_sun_time_.
   * @return Hour angle in rad.
   */
  double hour_angle_(double sun_time);

  double elevation_(double sun_time);

  double elevation_rad_(double sun_time);

  double zenith_rad_(double sun_time);

  double azimuth_rad_(double sun_time);

  double azimuth_(double sun_time);

  /** Return the sun time given by the time_ object.
   *
   * Sun time is defined as doubleing point day of year.
   * Integer part encodes the day of the year (1=January 1st)
   * Decimal part encodes time of day (1/24 = 1 hour)
   */
  double calc_sun_time_(const time::ESPTime &time);

  uint32_t calc_epoch_(time::ESPTime base, double sun_time);

  /** Calculate the sun time of day
   *
   * @param day_of_year
   * @param elevation
   * @param rising
   * @return
   */
  double sun_time_for_elevation_(int32_t day_of_year, double elevation, bool rising);

  double latitude_rad_();

  time::RealTimeClock *time_;
  /// Latitude in degrees, range: -90 to 90.
  double latitude_;
  /// Longitude in degrees, range: -180 to 180.
  double longitude_;
};

class SunTrigger : public Trigger<>, public PollingComponent, public Parented<Sun> {
 public:
  SunTrigger() : PollingComponent(1000) {}

  void set_sunrise(bool sunrise) { sunrise_ = sunrise; }
  void set_elevation(double elevation) { elevation_ = elevation; }

  void update() override {
    double current = this->parent_->elevation();
    if (isnan(current))
      return;

    bool crossed;
    if (this->sunrise_) {
      crossed = this->last_elevation_ <= this->elevation_ && this->elevation_ < current;
    } else {
      crossed = this->last_elevation_ >= this->elevation_ && this->elevation_ > current;
    }

    if (crossed && !isnan(this->last_elevation_)) {
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
    if (this->above_)
      return current > elevation;
    else
      return current < elevation;
  }

 protected:
  bool above_;
};

}  // namespace sun
}  // namespace esphome
