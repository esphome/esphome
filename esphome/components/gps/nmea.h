#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/defines.h"

namespace esphome::gps {

/// Longest NMEA line kept, without the line end; the standard limit is 80, some receivers go over it.
static constexpr size_t NMEA_MAX_LINE = 96;

/// Bits of GPSData::updated and GPSData::valid.
enum GPSField : uint16_t {
  GPS_FIELD_LOCATION = 1 << 0,
  GPS_FIELD_SPEED = 1 << 1,
  GPS_FIELD_COURSE = 1 << 2,
  GPS_FIELD_ALTITUDE = 1 << 3,
  GPS_FIELD_SATELLITES = 1 << 4,
  GPS_FIELD_HDOP = 1 << 5,
  GPS_FIELD_TIME = 1 << 6,
  GPS_FIELD_DATE = 1 << 7,
  GPS_FIELD_GEOID_SEPARATION = 1 << 8,
};
static constexpr uint8_t GPS_FIELD_COUNT = 9;
static_assert(GPS_FIELD_GEOID_SEPARATION == 1 << (GPS_FIELD_COUNT - 1), "GPS_FIELD_COUNT must match the highest field");

/// Values from GGA and RMC sentences as scaled integers; a name ending in eN is the value times 10^N.
struct GPSData {
  int32_t latitude_e7{0};   ///< degrees, north positive
  int32_t longitude_e7{0};  ///< degrees, east positive
  int32_t altitude_e3{0};   ///< meters above mean sea level
#ifdef USE_GPS_GEOID_SEPARATION
  int32_t geoid_separation_e3{0};  ///< meters from the WGS84 ellipsoid up to mean sea level
#endif
  uint32_t speed_e3{0};   ///< knots
  uint16_t course_e2{0};  ///< degrees from true north
  uint16_t hdop_e2{0};
  uint16_t year{0};
#ifdef USE_GPS_MILLISECOND
  uint16_t millisecond{0};  ///< of the time; 0 when the time has no fraction
#endif
  uint8_t month{0};
  uint8_t day{0};
  uint8_t hour{0};  ///< UTC
  uint8_t minute{0};
  uint8_t second{0};
  uint8_t satellites{0};
  uint16_t updated{0};  ///< GPSField bits set by the last sentence
  uint16_t valid{0};    ///< GPSField bits set by any sentence so far

  bool has(GPSField field) const { return (this->valid & field) != 0; }
  bool is_updated(GPSField field) const { return (this->updated & field) != 0; }

  // A double quotient rounds once to the float nearest the decimal value
  float latitude() const { return static_cast<float>(this->latitude_e7 / 1e7); }
  float longitude() const { return static_cast<float>(this->longitude_e7 / 1e7); }
  float altitude() const { return static_cast<float>(this->altitude_e3 / 1e3); }
  float speed_knots() const { return static_cast<float>(this->speed_e3 / 1e3); }
  float speed_kmh() const { return static_cast<float>(this->speed_e3 * 1852.0 / 1e6); }
  float course() const { return static_cast<float>(this->course_e2 / 1e2); }
  float hdop() const { return static_cast<float>(this->hdop_e2 / 1e2); }
#ifdef USE_GPS_GEOID_SEPARATION
  float geoid_separation() const { return static_cast<float>(this->geoid_separation_e3 / 1e3); }
#endif
};

/// Parse one NMEA line of len bytes, which is changed in place; the sentence starts at the last '$'. On a GGA or RMC
/// sentence with a good checksum and no malformed field, store its values in data and return true; otherwise leave
/// data unchanged and return false.
bool parse_nmea_sentence(char *line, size_t len, GPSData &data);

}  // namespace esphome::gps
