#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::gps {

/// Longest NMEA line kept, without the line end; the standard limit is 80, some receivers go over it.
static constexpr size_t NMEA_MAX_LINE = 96;

/// Bits of GPSData::updated and GPSData::valid.
enum GPSField : uint8_t {
  GPS_FIELD_LOCATION = 1 << 0,
  GPS_FIELD_SPEED = 1 << 1,
  GPS_FIELD_COURSE = 1 << 2,
  GPS_FIELD_ALTITUDE = 1 << 3,
  GPS_FIELD_SATELLITES = 1 << 4,
  GPS_FIELD_HDOP = 1 << 5,
  GPS_FIELD_TIME = 1 << 6,
  GPS_FIELD_DATE = 1 << 7,
};

/// Values from GGA and RMC sentences as scaled integers; a name ending in eN is the value times 10^N.
struct GPSData {
  int32_t latitude_e7{0};   ///< degrees, north positive
  int32_t longitude_e7{0};  ///< degrees, east positive
  int32_t altitude_e3{0};   ///< meters above mean sea level
  uint32_t speed_e3{0};     ///< knots
  uint16_t course_e2{0};    ///< degrees from true north
  uint16_t hdop_e2{0};
  uint16_t year{0};
  uint8_t month{0};
  uint8_t day{0};
  uint8_t hour{0};  ///< UTC
  uint8_t minute{0};
  uint8_t second{0};
  uint8_t satellites{0};
  uint8_t updated{0};  ///< GPSField bits set by the last sentence
  uint8_t valid{0};    ///< GPSField bits set by any sentence so far

  bool has(GPSField field) const { return (this->valid & field) != 0; }
  bool is_updated(GPSField field) const { return (this->updated & field) != 0; }

  float latitude() const { return this->latitude_e7 * 1e-7f; }
  float longitude() const { return this->longitude_e7 * 1e-7f; }
  float altitude() const { return this->altitude_e3 * 1e-3f; }
  float speed_knots() const { return this->speed_e3 * 1e-3f; }
  float speed_kmh() const { return this->speed_e3 * 1.852e-3f; }
  float course() const { return this->course_e2 * 1e-2f; }
  float hdop() const { return this->hdop_e2 * 1e-2f; }
};

/// Parse one NMEA line, which is changed in place. On a GGA or RMC sentence with a good checksum and no malformed
/// field, store its values in data and return true; otherwise leave data unchanged and return false.
bool parse_nmea_sentence(char *line, GPSData &data);

}  // namespace esphome::gps
