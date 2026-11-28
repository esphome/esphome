#include "nmea.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"

#include <cstring>
#include <limits>

namespace esphome::gps {

ESPHOME_LOG_TAG(TAG, "gps.nmea");

static constexpr size_t MAX_FIELDS = 20;
static constexpr int32_t E7 = 10000000;

namespace {

class Fields {
 public:
  /// Split line at each comma; fields past MAX_FIELDS are ignored.
  explicit Fields(char *line) {
    this->fields_[0] = line;
    for (char *p = line; *p != '\0' && this->count_ < MAX_FIELDS; p++) {
      if (*p == ',') {
        *p = '\0';
        this->fields_[this->count_++] = p + 1;
      }
    }
  }
  /// The field, or an empty string when the sentence is shorter.
  const char *operator[](size_t i) const { return i < this->count_ ? this->fields_[i] : ""; }

 protected:
  const char *fields_[MAX_FIELDS];
  size_t count_{1};
};

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool all_digits(const char *s) {
  for (; *s != '\0'; s++) {
    if (!is_digit(*s))
      return false;
  }
  return true;
}

/// Parse [-]digits[.digits] scaled by 10^decimals; decimals past that are dropped. With decimals 0, no fraction.
bool parse_decimal(const char *s, uint8_t decimals, int32_t &out) {
  const bool integer = decimals == 0;
  const bool negative = *s == '-';
  if (negative)
    s++;
  uint32_t value = 0;
  bool any_digit = false;
  bool fraction = false;
  for (; *s != '\0'; s++) {
    if (*s == '.' && !fraction && !integer) {
      fraction = true;
      continue;
    }
    if (!is_digit(*s))
      return false;
    if (fraction) {
      if (decimals == 0)
        continue;
      decimals--;
    }
    if (value > (INT32_MAX - 9) / 10)
      return false;
    value = value * 10 + (*s - '0');
    any_digit = true;
  }
  if (!any_digit)
    return false;
  for (; decimals > 0; decimals--) {
    if (value > INT32_MAX / 10)
      return false;
    value *= 10;
  }
  out = negative ? -static_cast<int32_t>(value) : static_cast<int32_t>(value);
  return true;
}

template<typename T> bool parse_unsigned(const char *s, uint8_t decimals, T &out) {
  int32_t value;
  if (!parse_decimal(s, decimals, value) || value < 0 || static_cast<uint32_t>(value) > std::numeric_limits<T>::max())
    return false;
  out = static_cast<T>(value);
  return true;
}

bool parse_two_digits(const char *s, uint8_t max, uint8_t &out) {
  if (!is_digit(s[0]) || !is_digit(s[1]))
    return false;
  out = (s[0] - '0') * 10 + (s[1] - '0');
  return out <= max;
}

/// Parse [d]ddmm[.mmmm] and its N/S or E/W field into degrees times 10^7.
bool parse_coordinate(const char *s, const char *hemisphere, char positive, char negative, int32_t max_degrees,
                      int32_t &out) {
  const char *dot = strchr(s, '.');
  const size_t int_len = dot != nullptr ? dot - s : strlen(s);
  if (int_len < 3 || int_len > 5)
    return false;
  int32_t degrees = 0;
  for (size_t i = 0; i != int_len; i++) {
    if (!is_digit(s[i]))
      return false;
    if (i < int_len - 2)
      degrees = degrees * 10 + (s[i] - '0');
  }
  int32_t minutes_e7;
  if (degrees > max_degrees || !parse_decimal(s + int_len - 2, 7, minutes_e7) || minutes_e7 >= 60 * E7)
    return false;
  const int32_t value = degrees * E7 + (minutes_e7 + 30) / 60;
  if (value > max_degrees * E7 || strlen(hemisphere) != 1)
    return false;
  if (hemisphere[0] == positive) {
    out = value;
  } else if (hemisphere[0] == negative) {
    out = -value;
  } else {
    return false;
  }
  return true;
}

/// Parse hhmmss[.s...]; digits past milliseconds are dropped.
bool parse_time(const char *s, GPSData &data) {
  if (!parse_two_digits(s, 23, data.hour) || !parse_two_digits(s + 2, 59, data.minute) ||
      !parse_two_digits(s + 4, 60, data.second))
    return false;
#ifdef USE_GPS_MILLISECOND
  data.millisecond = 0;
#endif
  if (s[6] == '\0')
    return true;
  if (s[6] != '.' || s[7] == '\0' || !all_digits(s + 7))
    return false;
#ifdef USE_GPS_MILLISECOND
  uint16_t scale = 100;
  for (const char *p = s + 7; *p != '\0' && scale != 0; p++, scale /= 10)
    data.millisecond += (*p - '0') * scale;
#endif
  return true;
}

/// Parse ddmmyy.
bool parse_date(const char *s, GPSData &data) {
  uint8_t year;
  if (strlen(s) != 6 || !parse_two_digits(s, 31, data.day) || !parse_two_digits(s + 2, 12, data.month) ||
      !parse_two_digits(s + 4, 99, year) || data.month == 0)
    return false;
  data.year = 2000 + year;
  return data.day != 0 && data.day <= days_in_month(data.month, data.year);
}

/// Parse the four location fields starting at index first, when they are present; all empty means no location.
bool parse_location(const Fields &f, size_t first, GPSData &data) {
  if (*f[first] == '\0' && *f[first + 1] == '\0' && *f[first + 2] == '\0' && *f[first + 3] == '\0')
    return true;
  if (!parse_coordinate(f[first], f[first + 1], 'N', 'S', 90, data.latitude_e7) ||
      !parse_coordinate(f[first + 2], f[first + 3], 'E', 'W', 180, data.longitude_e7))
    return false;
  data.updated |= GPS_FIELD_LOCATION;
  return true;
}

/// Parse an optional field with parse(); an empty field leaves the value as it was.
template<typename P> bool parse_optional(const char *s, GPSField field, GPSData &data, P &&parse) {
  if (*s == '\0')
    return true;
  if (!parse(s))
    return false;
  data.updated |= field;
  return true;
}

// $--GGA,time,lat,N,lon,E,quality,satellites,hdop,altitude,M,geoid,M,age,station*hh
bool parse_gga(const Fields &f, GPSData &data) {
  uint8_t quality = 0;
  if (*f[6] != '\0' && !parse_unsigned(f[6], 0, quality))
    return false;
  if (!parse_optional(f[7], GPS_FIELD_SATELLITES, data,
                      [&data](const char *s) { return parse_unsigned(s, 0, data.satellites); }) ||
      !parse_optional(f[8], GPS_FIELD_HDOP, data,
                      [&data](const char *s) { return parse_unsigned(s, 2, data.hdop_e2); }))
    return false;
  // Without a fix the time can come from the receiver's own clock, which may be wrong
  if (quality == 0)
    return true;
  if (!parse_optional(f[1], GPS_FIELD_TIME, data, [&data](const char *s) { return parse_time(s, data); }) ||
      !parse_location(f, 2, data) || !parse_optional(f[9], GPS_FIELD_ALTITUDE, data, [&data](const char *s) {
        return parse_decimal(s, 3, data.altitude_e3);
      }))
    return false;
#ifdef USE_GPS_GEOID_SEPARATION
  return parse_optional(f[11], GPS_FIELD_GEOID_SEPARATION, data,
                        [&data](const char *s) { return parse_decimal(s, 3, data.geoid_separation_e3); });
#else
  return true;
#endif
}

// $--RMC,time,status,lat,N,lon,E,speed,course,date,variation,E[,mode[,nav status]]*hh
bool parse_rmc(const Fields &f, GPSData &data) {
  // Without a fix the time and date can come from the receiver's own clock, which may be wrong
  if (*f[2] == '\0' || strcmp(f[2], "V") == 0)
    return true;
  if (strcmp(f[2], "A") != 0)
    return false;
  return parse_optional(f[1], GPS_FIELD_TIME, data, [&data](const char *s) { return parse_time(s, data); }) &&
         parse_location(f, 3, data) &&
         parse_optional(f[7], GPS_FIELD_SPEED, data,
                        [&data](const char *s) { return parse_unsigned(s, 3, data.speed_e3); }) &&
         parse_optional(
             f[8], GPS_FIELD_COURSE, data,
             [&data](const char *s) { return parse_unsigned(s, 2, data.course_e2) && data.course_e2 <= 36000; }) &&
         parse_optional(f[9], GPS_FIELD_DATE, data, [&data](const char *s) { return parse_date(s, data); });
}

}  // namespace

bool parse_nmea_sentence(char *line, size_t len, GPSData &data) {
  // Noise before the sentence, NUL bytes included, is skipped
  size_t pos = len;
  while (pos != 0 && line[pos - 1] != '$')
    pos--;
  if (pos == 0 || strlen(line + pos - 1) != len - pos + 1)
    return false;
  char *start = line + pos - 1;
  char *star = strchr(start, '*');
  uint8_t checksum;
  if (star == nullptr || strlen(star + 1) != 2 || !parse_hex(star + 1, &checksum, 1))
    return false;
  if (xor_bytes(reinterpret_cast<const uint8_t *>(start + 1), star - start - 1) != checksum) {
    ESP_LOGV(TAG, "Bad checksum: %s", start);
    return false;
  }
  *star = '\0';
  const Fields fields(start + 1);
  const char *address = fields[0];
  // Two talker characters and the sentence type; proprietary sentences start with P
  if (strlen(address) != 5 || address[0] == 'P')
    return false;
  const char *type = address + 2;
  GPSData next = data;
  next.updated = 0;
  bool ok;
  if (strcmp(type, "GGA") == 0) {
    ok = parse_gga(fields, next);
  } else if (strcmp(type, "RMC") == 0) {
    ok = parse_rmc(fields, next);
  } else {
    return false;
  }
  if (!ok) {
    ESP_LOGV(TAG, "Malformed %s sentence", address);
    return false;
  }
  next.valid |= next.updated;
  data = next;
  return true;
}

}  // namespace esphome::gps
