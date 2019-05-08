#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace sun {

static const float PI_F = 3.141592653589793f;
static const float TAU_F = 6.283185307179586f;
static const float TO_RADIANS = PI_F / 180.0f;
static const float TO_DEGREES = 180.0f / PI_F;
static const float EARTH_TILT = 23.44f * TO_RADIANS;

class Sun {
 public:
  optional<time::ESPTime> sunrise(float elevation = 0.0f) {
    auto time = this->time_->utcnow();
    float sun_time = this->sun_time_for_elevation_(time.day_of_year, elevation, true);
    if (isnan(sun_time))
      return {};
    uint32_t epoch = this->calc_epoch_(time, sun_time);
    return time::ESPTime::from_epoch_local(epoch);
  }

  optional<time::ESPTime> dawn(float elevation = 0.0f) {
    auto time = this->time_->utcnow();
    float sun_time = this->sun_time_for_elevation_(time.day_of_year, elevation, false);
    if (isnan(sun_time))
      return {};
    uint32_t epoch = this->calc_epoch_(time, sun_time);
    return time::ESPTime::from_epoch_local(epoch);
  }

  float elevation() {
    return this->elevation_(this->calc_sun_time_(this->time_->utcnow()));
  }

  float azimuth() {
    return this->azimuth_(this->calc_sun_time_(this->time_->utcnow()));
  }

 protected:
  /** Calculate the declination of the sun in rad.
   *
   * See https://en.wikipedia.org/wiki/Position_of_the_Sun#Declination_of_the_Sun_as_seen_from_Earth
   *
   * Accuracy: ±0.2°
   *
   * @param sun_time The day of the year, 1 means January 1st. See calc_sun_time_.
   * @return Sun declination in degrees
   */
  float sun_declination_(float sun_time) {
    float n = sun_time - 1.0f;
    // maximum declination
    const float tot = -sinf(EARTH_TILT);

    // eccentricity of the earth's orbit (ellipse)
    float eccentricity = 0.0167f;

    // days since perihelion (January 3rd)
    float days_since_perihelion = n - 2;
    // days since december solstice (december 22)
    float days_since_december_solstice = n + 10;
    const float c = TAU_F / 365.24f;
    float v = cosf(c * days_since_december_solstice + 2 * eccentricity * sinf(c * days_since_perihelion));
    // Make sure value is in range (float error may lead to results slightly larger than 1)
    float x = clamp(tot * v, 0, 1);
    return asinf(x);
  }

  float elevation_ratio_(float sun_time) {
    float decl = this->sun_declination_(sun_time);
    float hangle = this->hour_angle_(sun_time);
    float val = sinf(this->latitude_) * sinf(decl) + cosf(this->latitude_)*cosf(decl)*cosf(hangle);
    val = clamp(val, 0.0f, 1.0f);
    return val;
  }

  /** Calculate the hour angle based on the sun time of day in hours.
   *
   * Positive in morning, 0 at noon, negative in afternoon.
   *
   * @param sun_time Sun time, see calc_sun_time_.
   * @return Hour angle in rad.
   */
  float hour_angle_(float sun_time) {
    float time_of_day = fmodf(sun_time, 1.0f) * 24.0f;
    return -PI_F * (time_of_day - 12) / 12;
  }

  float elevation_(float sun_time) {
    return this->elevation_rad_(sun_time) * TO_DEGREES;
  }

  float elevation_rad_(float sun_time) {
    return asinf(this->elevation_ratio_(sun_time));
  }

  float zenith_rad_(float sun_time) {
    return acosf(this->elevation_ratio_(sun_time));
  }

  float azimuth_rad_(float sun_time) {
    float hangle = -this->hour_angle_(sun_time);
    float decl = this->sun_declination_(sun_time);
    float zen = this->zenith_rad_(sun_time);
    float nom = cosf(zen)*sinf(this->latitude_)-sinf(decl);
    float denom = sinf(zen)*cosf(this->latitude_);
    float v = clamp(nom/denom, -1.0, 1.0);
    float az = PI_F - acosf(v);
    if (hangle > 0)
      az = -az;
    if (az < 0)
      az += TAU_F;
    return az;
  }

  float azimuth_(float sun_time) {
    return this->azimuth_rad_(sun_time) * TO_DEGREES;
  }

  /** Return the sun time given by the time_ object.
   *
   * Sun time is defined as floating point day of year.
   * Integer part encodes the day of the year (1=January 1st)
   * Decimal part encodes time of day (1/24 = 1 hour)
   */
  float calc_sun_time_(const time::ESPTime &time) {
    // Time as seen at 0° longitude
    float base = (
        time.day_of_year +
        time.hour / 24.0f +
        time.minute / 24.0f / 60.0f +
        time.second / 24.0f / 60.0f / 60.0f
    );
    // Add longitude correction
    float add = this->longitude_ / 360.0f;
    return base + add;
  }

  uint32_t calc_epoch_(const time::ESPTime &base, float sun_time) {
    uint32_t epoch_offset = (base.day_of_year - 1) * 24*60*60 + base.hour * 60*60 + base.minute * 60 + base.second;
    uint32_t jan1_epoch = base.time - epoch_offset;
    return jan1_epoch + static_cast<uint32_t>(sun_time*24*60*60);
  }

  /** Calculate the sun time of day
   *
   * @param day_of_year
   * @param elevation
   * @param rising
   * @return
   */
  float sun_time_for_elevation_(int32_t day_of_year, float elevation, bool rising) {
    // Use binary search, newton's method would be better but binary search already
    // converges quite well (19 cycles) and much simpler. Function is guaranteed to be
    // monotonous.
    float lo, hi;
    if (rising) {
      lo = day_of_year + 0.0f;
      hi = day_of_year + 0.5f;
    } else {
      lo = day_of_year + 1.0f;
      hi = day_of_year + 0.5f;
    }

    float min_elevation = this->elevation_(lo);
    float max_elevation = this->elevation_(hi);
    if (elevation < min_elevation || elevation > max_elevation)
      return NAN;

    // Accuracy: 0.1s
    const float accuracy = 24.0f*60.0f*60.0f*10.0f;

    while (fabsf(hi - lo) > 1/accuracy) {
      float mid = (lo + hi) / 2.0f;
      float value = this->elevation_(mid) - elevation;
      if (value < 0) {
        lo = mid;
      } else if (value > 0) {
        hi = mid;
      } else {
        lo = hi = mid;
        break;
      }
    }

    return (lo + hi) / 2.0f;
  }

  time::RealTimeClock *time_;
  float latitude_;
  float longitude_;
};

}  // namespace sun
}  // namespace esphome
