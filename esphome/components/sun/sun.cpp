#include "sun.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sun {

static const char *TAG = "sun";

#undef PI

/* Usually, ESPHome uses single-precision floating point values
 * because those tend to be accurate enough and are more efficient.
 *
 * However, some of the data in this class has to be quite accurate, so double is
 * used everywhere.
 */
static const double PI = 3.141592653589793;
static const double TAU = 6.283185307179586;
static const double TO_RADIANS = PI / 180.0;
static const double TO_DEGREES = 180.0 / PI;
static const double EARTH_TILT = 23.44 * TO_RADIANS;

optional<time::ESPTime> Sun::sunrise(double elevation) {
  auto time = this->time_->now();
  if (!time.is_valid())
    return {};
  double sun_time = this->sun_time_for_elevation_(time.day_of_year, elevation, true);
  if (isnan(sun_time))
    return {};
  uint32_t epoch = this->calc_epoch_(time, sun_time);
  return time::ESPTime::from_epoch_local(epoch);
}
optional<time::ESPTime> Sun::sunset(double elevation) {
  auto time = this->time_->now();
  if (!time.is_valid())
    return {};
  double sun_time = this->sun_time_for_elevation_(time.day_of_year, elevation, false);
  if (isnan(sun_time))
    return {};
  uint32_t epoch = this->calc_epoch_(time, sun_time);
  return time::ESPTime::from_epoch_local(epoch);
}
double Sun::elevation() {
  auto time = this->current_sun_time_();
  if (isnan(time))
    return NAN;
  return this->elevation_(time);
}
double Sun::azimuth() {
  auto time = this->current_sun_time_();
  if (isnan(time))
    return NAN;
  return this->azimuth_(time);
}
// like clamp, but with doubles
double clampd(double val, double min, double max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}
double Sun::sun_declination_(double sun_time) {
  double n = sun_time - 1.0;
  // maximum declination
  const double tot = -sin(EARTH_TILT);

  // eccentricity of the earth's orbit (ellipse)
  double eccentricity = 0.0167;

  // days since perihelion (January 3rd)
  double days_since_perihelion = n - 2;
  // days since december solstice (december 22)
  double days_since_december_solstice = n + 10;
  const double c = TAU / 365.24;
  double v = cos(c * days_since_december_solstice + 2 * eccentricity * sin(c * days_since_perihelion));
  // Make sure value is in range (double error may lead to results slightly larger than 1)
  double x = clampd(tot * v, -1.0, 1.0);
  return asin(x);
}
double Sun::elevation_ratio_(double sun_time) {
  double decl = this->sun_declination_(sun_time);
  double hangle = this->hour_angle_(sun_time);
  double a = sin(this->latitude_rad_()) * sin(decl);
  double b = cos(this->latitude_rad_()) * cos(decl) * cos(hangle);
  double val = clampd(a + b, -1.0, 1.0);
  return val;
}
double Sun::latitude_rad_() { return this->latitude_ * TO_RADIANS; }
double Sun::hour_angle_(double sun_time) {
  double time_of_day = fmod(sun_time, 1.0) * 24.0;
  return -PI * (time_of_day - 12) / 12;
}
double Sun::elevation_(double sun_time) { return this->elevation_rad_(sun_time) * TO_DEGREES; }
double Sun::elevation_rad_(double sun_time) { return asin(this->elevation_ratio_(sun_time)); }
double Sun::zenith_rad_(double sun_time) { return acos(this->elevation_ratio_(sun_time)); }
double Sun::azimuth_rad_(double sun_time) {
  double hangle = -this->hour_angle_(sun_time);
  double decl = this->sun_declination_(sun_time);
  double zen = this->zenith_rad_(sun_time);
  double nom = cos(zen) * sin(this->latitude_rad_()) - sin(decl);
  double denom = sin(zen) * cos(this->latitude_rad_());
  double v = clampd(nom / denom, -1.0, 1.0);
  double az = PI - acos(v);
  if (hangle > 0)
    az = -az;
  if (az < 0)
    az += TAU;
  return az;
}
double Sun::azimuth_(double sun_time) { return this->azimuth_rad_(sun_time) * TO_DEGREES; }
double Sun::calc_sun_time_(const time::ESPTime &time) {
  // Time as seen at 0° longitude
  if (!time.is_valid())
    return NAN;

  double base = (time.day_of_year + time.hour / 24.0 + time.minute / 24.0 / 60.0 + time.second / 24.0 / 60.0 / 60.0);
  // Add longitude correction
  double add = this->longitude_ / 360.0;
  return base + add;
}
uint32_t Sun::calc_epoch_(time::ESPTime base, double sun_time) {
  sun_time -= this->longitude_ / 360.0;
  base.day_of_year = uint32_t(floor(sun_time));

  sun_time = (sun_time - base.day_of_year) * 24.0;
  base.hour = uint32_t(floor(sun_time));

  sun_time = (sun_time - base.hour) * 60.0;
  base.minute = uint32_t(floor(sun_time));

  sun_time = (sun_time - base.minute) * 60.0;
  base.second = uint32_t(floor(sun_time));

  base.recalc_timestamp_utc(true);
  return base.timestamp;
}
double Sun::sun_time_for_elevation_(int32_t day_of_year, double elevation, bool rising) {
  // Use binary search, newton's method would be better but binary search already
  // converges quite well (19 cycles) and much simpler. Function is guaranteed to be
  // monotonous.
  double lo, hi;
  if (rising) {
    lo = day_of_year + 0.0;
    hi = day_of_year + 0.5;
  } else {
    lo = day_of_year + 1.0;
    hi = day_of_year + 0.5;
  }

  double min_elevation = this->elevation_(lo);
  double max_elevation = this->elevation_(hi);
  if (elevation < min_elevation || elevation > max_elevation)
    return NAN;

  // Accuracy: 0.1s
  const double accuracy = 1.0 / (24.0 * 60.0 * 60.0 * 10.0);

  while (fabs(hi - lo) > accuracy) {
    double mid = (lo + hi) / 2.0;
    double value = this->elevation_(mid) - elevation;
    if (value < 0) {
      lo = mid;
    } else if (value > 0) {
      hi = mid;
    } else {
      lo = hi = mid;
      break;
    }
  }

  return (lo + hi) / 2.0;
}

}  // namespace sun
}  // namespace esphome
