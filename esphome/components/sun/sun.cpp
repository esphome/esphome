#include "sun.h"
#include "esphome/core/log.h"

/*
The formulas/algorithms in this module are based on the book
"Astronomical algorithms" by Jean Meeus (2nd edition)

The target accuracy of this implementation is ~1min for sunrise/sunset calculations,
and 6 arcminutes for elevation/azimuth. As such, some of the advanced correction factors
like exact nutation are not included. But in some testing the accuracy appears to be within range
for random spots around the globe.
*/

namespace esphome {
namespace sun {

using namespace esphome::sun::internal;

static const char *const TAG = "sun";

#undef PI
#undef degrees
#undef radians
#undef sq

static const num_t PI = 3.141592653589793;
inline num_t degrees(num_t rad) { return rad * 180 / PI; }
inline num_t radians(num_t deg) { return deg * PI / 180; }
inline num_t arcdeg(num_t deg, num_t minutes, num_t seconds) { return deg + minutes / 60 + seconds / 3600; }
inline num_t sq(num_t x) { return x * x; }
inline num_t cb(num_t x) { return x * x * x; }

num_t GeoLocation::latitude_rad() const { return radians(latitude); }
num_t GeoLocation::longitude_rad() const { return radians(longitude); }
num_t EquatorialCoordinate::right_ascension_rad() const { return radians(right_ascension); }
num_t EquatorialCoordinate::declination_rad() const { return radians(declination); }
num_t HorizontalCoordinate::elevation_rad() const { return radians(elevation); }
num_t HorizontalCoordinate::azimuth_rad() const { return radians(azimuth); }

num_t julian_day(ESPTime moment) {
  // p. 59
  // UT -> JD, TT -> JDE
  int y = moment.year;
  int m = moment.month;
  num_t d = moment.day_of_month;
  d += moment.hour / 24.0;
  d += moment.minute / (24.0 * 60);
  d += moment.second / (24.0 * 60 * 60);
  if (m <= 2) {
    y -= 1;
    m += 12;
  }
  int a = y / 100;
  int b = 2 - a + a / 4;
  return ((int) (365.25 * (y + 4716))) + ((int) (30.6001 * (m + 1))) + d + b - 1524.5;
}
num_t delta_t(ESPTime moment) {
  // approximation for 2005-2050 from NASA (https://eclipse.gsfc.nasa.gov/SEhelp/deltatpoly2004.html)
  int t = moment.year - 2000;
  return 62.92 + t * (0.32217 + t * 0.005589);
}
// Perform a fractional module operation where the result will always be positive (wrapping around)
num_t wmod(num_t x, num_t y) {
  num_t res = fmod(x, y);
  if (res < 0)
    res += y;
  return res;
}

num_t internal::Moment::jd() const { return julian_day(dt); }

num_t internal::Moment::jde() const {
  // dt is in UT1, but JDE is based on TT
  // so add deltaT factor
  return jd() + delta_t(dt) / (60 * 60 * 24);
}

struct SunAtTime {
  num_t jde;
  num_t t;

  // eq 25.1, p. 163; julian centuries from the epoch J2000.0
  SunAtTime(num_t jde) : jde(jde), t((jde - 2451545) / 36525.0) {}

  num_t mean_obliquity() const {
    // eq. 22.2, p. 147; mean obliquity of the ecliptic
    num_t epsilon_0 = (+arcdeg(23, 26, 21.448) - arcdeg(0, 0, 46.8150) * t - arcdeg(0, 0, 0.00059) * sq(t) +
                       arcdeg(0, 0, 0.001813) * cb(t));
    return epsilon_0;
  }

  num_t omega() const {
    // eq. 25.8, p. 165; correction factor for obliquity of the ecliptic
    // in degrees
    num_t omega = 125.05 - 1934.136 * t;
    return omega;
  }

  num_t true_obliquity() const {
    // eq. 25.8, p. 165; correction factor for obliquity of the ecliptic
    num_t delta_epsilon = 0.00256 * cos(radians(omega()));
    num_t epsilon = mean_obliquity() + delta_epsilon;
    return epsilon;
  }

  num_t mean_longitude() const {
    // eq 25.2, p. 163; geometric mean longitude = mean equinox of the date in degrees
    num_t l0 = 280.46646 + 36000.76983 * t + 0.0003032 * sq(t);
    return wmod(l0, 360);
  }

  num_t eccentricity() const {
    // eq 25.4, p. 163; eccentricity of earth's orbit
    num_t e = 0.016708634 - 0.000042037 * t - 0.0000001267 * sq(t);
    return e;
  }

  num_t mean_anomaly() const {
    // eq 25.3, p. 163; mean anomaly of the sun in degrees
    num_t m = 357.52911 + 35999.05029 * t - 0.0001537 * sq(t);
    return wmod(m, 360);
  }

  num_t equation_of_center() const {
    // p. 164; sun's equation of the center c in degrees
    num_t m_rad = radians(mean_anomaly());
    num_t c = ((1.914602 - 0.004817 * t - 0.000014 * sq(t)) * sin(m_rad) + (0.019993 - 0.000101 * t) * sin(2 * m_rad) +
               0.000289 * sin(3 * m_rad));
    return wmod(c, 360);
  }

  num_t true_longitude() const {
    // p. 164; sun's true longitude in degrees
    num_t x = mean_longitude() + equation_of_center();
    return wmod(x, 360);
  }

  num_t true_anomaly() const {
    // p. 164; sun's true anomaly in degrees
    num_t x = mean_anomaly() + equation_of_center();
    return wmod(x, 360);
  }

  num_t apparent_longitude() const {
    // p. 164; sun's apparent longitude = true equinox in degrees
    num_t x = true_longitude() - 0.00569 - 0.00478 * sin(radians(omega()));
    return wmod(x, 360);
  }

  EquatorialCoordinate equatorial_coordinate() const {
    num_t epsilon_rad = radians(true_obliquity());
    // eq. 25.6; p. 165; sun's right ascension alpha
    num_t app_lon_rad = radians(apparent_longitude());
    num_t right_ascension_rad = atan2(cos(epsilon_rad) * sin(app_lon_rad), cos(app_lon_rad));
    num_t declination_rad = asin(sin(epsilon_rad) * sin(app_lon_rad));
    return EquatorialCoordinate{degrees(right_ascension_rad), degrees(declination_rad)};
  }

  num_t equation_of_time() const {
    // chapter 28, p. 185
    num_t epsilon_half = radians(true_obliquity() / 2);
    num_t y = sq(tan(epsilon_half));
    num_t l2 = 2 * mean_longitude();
    num_t l2_rad = radians(l2);
    num_t e = eccentricity();
    num_t m = mean_anomaly();
    num_t m_rad = radians(m);
    num_t sin_m = sin(m_rad);
    num_t eot = (y * sin(l2_rad) - 2 * e * sin_m + 4 * e * y * sin_m * cos(l2_rad) - 1 / 2.0 * sq(y) * sin(2 * l2_rad) -
                 5 / 4.0 * sq(e) * sin(2 * m_rad));
    return degrees(eot);
  }

  void debug() const {
    // debug output like in example 25.a, p. 165
    ESP_LOGV(TAG, "jde: %f", jde);
    ESP_LOGV(TAG, "T: %f", t);
    ESP_LOGV(TAG, "L_0: %f", mean_longitude());
    ESP_LOGV(TAG, "M: %f", mean_anomaly());
    ESP_LOGV(TAG, "e: %f", eccentricity());
    ESP_LOGV(TAG, "C: %f", equation_of_center());
    ESP_LOGV(TAG, "Odot: %f", true_longitude());
    ESP_LOGV(TAG, "Omega: %f", omega());
    ESP_LOGV(TAG, "lambda: %f", apparent_longitude());
    ESP_LOGV(TAG, "epsilon_0: %f", mean_obliquity());
    ESP_LOGV(TAG, "epsilon: %f", true_obliquity());
    ESP_LOGV(TAG, "v: %f", true_anomaly());
    auto eq = equatorial_coordinate();
    ESP_LOGV(TAG, "right_ascension: %f", eq.right_ascension);
    ESP_LOGV(TAG, "declination: %f", eq.declination);
  }
};

struct SunAtLocation {
  GeoLocation location;

  num_t greenwich_sidereal_time(Moment moment) const {
    // Return the greenwich mean sidereal time for this instant in degrees
    // see chapter 12, p. 87
    num_t jd = moment.jd();
    // eq 12.1, p.87; jd for 0h UT of this date
    ESPTime moment_0h = moment.dt;
    moment_0h.hour = moment_0h.minute = moment_0h.second = 0;
    num_t jd0 = Moment{moment_0h}.jd();
    num_t t = (jd0 - 2451545) / 36525;
    // eq. 12.4, p.88
    num_t gmst = (+280.46061837 + 360.98564736629 * (jd - 2451545) + 0.000387933 * sq(t) - (1 / 38710000.0) * cb(t));
    return wmod(gmst, 360);
  }

  HorizontalCoordinate true_coordinate(Moment moment) const {
    auto eq = SunAtTime(moment.jde()).equatorial_coordinate();
    num_t gmst = greenwich_sidereal_time(moment);
    // do not apply any nutation correction (not important for our target accuracy)
    num_t nutation_corr = 0;

    num_t ra = eq.right_ascension;
    num_t alpha = gmst + nutation_corr + location.longitude - ra;
    alpha = wmod(alpha, 360);
    num_t alpha_rad = radians(alpha);

    num_t sin_lat = sin(location.latitude_rad());
    num_t cos_lat = cos(location.latitude_rad());
    num_t sin_elevation = (+sin_lat * sin(eq.declination_rad()) + cos_lat * cos(eq.declination_rad()) * cos(alpha_rad));
    num_t elevation_rad = asin(sin_elevation);
    num_t azimuth_rad = atan2(sin(alpha_rad), cos(alpha_rad) * sin_lat - tan(eq.declination_rad()) * cos_lat);
    return HorizontalCoordinate{degrees(elevation_rad), degrees(azimuth_rad) + 180};
  }

  optional<ESPTime> sunrise(ESPTime date, num_t zenith) const { return event(true, date, zenith); }
  optional<ESPTime> sunset(ESPTime date, num_t zenith) const { return event(false, date, zenith); }
  optional<ESPTime> event(bool rise, ESPTime date, num_t zenith) const {
    // couldn't get the method described in chapter 15 to work,
    // so instead this is based on the algorithm in time4j
    // https://github.com/MenoData/Time4J/blob/master/base/src/main/java/net/time4j/calendar/astro/StdSolarCalculator.java
    auto m = local_event_(date, 12);  // noon
    num_t jde = julian_day(m);
    num_t new_h = 0, old_h;
    do {
      old_h = new_h;
      auto x = local_hour_angle_(jde + old_h / 86400, rise, zenith);
      if (!x.has_value())
        return {};
      new_h = *x;
    } while (std::abs(new_h - old_h) >= 15);
    time_t new_timestamp = m.timestamp + (time_t) new_h;
    return ESPTime::from_epoch_local(new_timestamp);
  }

 protected:
  optional<num_t> local_hour_angle_(num_t jde, bool rise, num_t zenith) const {
    auto pos = SunAtTime(jde).equatorial_coordinate();
    num_t dec_rad = pos.declination_rad();
    num_t lat_rad = location.latitude_rad();
    num_t num = cos(radians(zenith)) - (sin(dec_rad) * sin(lat_rad));
    num_t denom = cos(dec_rad) * cos(lat_rad);
    num_t cos_h = num / denom;
    if (cos_h > 1 || cos_h < -1)
      return {};
    num_t hour_angle = degrees(acos(cos_h)) * 240;
    if (rise)
      hour_angle *= -1;
    return hour_angle;
  }

  ESPTime local_event_(ESPTime date, int hour) const {
    // input date should be in UTC, and hour/minute/second fields 0
    num_t added_d = hour / 24.0 - location.longitude / 360;
    num_t jd = julian_day(date) + added_d;

    num_t eot = SunAtTime(jd).equation_of_time() * 240;
    time_t new_timestamp = (time_t) (date.timestamp + added_d * 86400 - eot);
    return ESPTime::from_epoch_utc(new_timestamp);
  }
};

HorizontalCoordinate Sun::calc_coords_() {
  SunAtLocation sun{location_};
  Moment m{time_->utcnow()};
  if (!m.dt.is_valid())
    return HorizontalCoordinate{NAN, NAN};

  // uncomment to print some debug output
  /*
  SunAtTime st{m.jde()};
  st.debug();
  */
  return sun.true_coordinate(m);
}
optional<ESPTime> Sun::calc_event_(ESPTime date, bool rising, double zenith) {
  SunAtLocation sun{location_};
  if (!date.is_valid())
    return {};
  // Calculate UT1 timestamp at 0h
  auto today = date;
  today.hour = today.minute = today.second = 0;
  today.recalc_timestamp_utc();

  auto it = sun.event(rising, today, zenith);
  if (it.has_value() && it->timestamp < date.timestamp) {
    // We're calculating *next* sunrise/sunset, but calculated event
    // is today, so try again tomorrow
    time_t new_timestamp = today.timestamp + 24 * 60 * 60;
    today = ESPTime::from_epoch_utc(new_timestamp);
    it = sun.event(rising, today, zenith);
  }
  return it;
}
optional<ESPTime> Sun::calc_event_(bool rising, double zenith) {
  auto it = Sun::calc_event_(this->time_->utcnow(), rising, zenith);
  return it;
}

optional<ESPTime> Sun::sunrise(double elevation) { return this->calc_event_(true, 90 - elevation); }
optional<ESPTime> Sun::sunset(double elevation) { return this->calc_event_(false, 90 - elevation); }
optional<ESPTime> Sun::sunrise(ESPTime date, double elevation) { return this->calc_event_(date, true, 90 - elevation); }
optional<ESPTime> Sun::sunset(ESPTime date, double elevation) { return this->calc_event_(date, false, 90 - elevation); }
double Sun::elevation() { return this->calc_coords_().elevation; }
double Sun::azimuth() { return this->calc_coords_().azimuth; }

}  // namespace sun
}  // namespace esphome
