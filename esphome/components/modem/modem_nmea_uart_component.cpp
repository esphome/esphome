#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "modem_nmea_uart_component.h"
#include "modem_component.h"
#include "helpers.h"
#include <cmath>
#include <cstring>
#ifdef USE_ESP_IDF
namespace esphome {
namespace modem {

static const char *const TAG = "modem.nmea_uart";

struct GnssInfo {
  double lat_deg = NAN;
  double lon_deg = NAN;
  double alt_m = NAN;
  double hdop = NAN;
  double cog_deg = NAN;
  double spd = NAN;  // knots (normalized)
  int sat_used = 0;
  bool fix_valid = false;
  int hh = 0, mm = 0, ss = 0;
  int dd = 1, mo = 1, yy = 0;  // yy: two digits
};

// some functions are maybe_unused, because #ifdef are used in parse_gnssinfo
[[maybe_unused]] static inline bool to_double(const char *s, double &v) {
  if (s == nullptr || *s == '\0')
    return false;
  char *end = nullptr;
  v = std::strtod(s, &end);
  return end && end != s;
}

[[maybe_unused]] static inline bool to_int(const char *s, int &v) {
  if (s == nullptr || *s == '\0')
    return false;
  char *end = nullptr;
  int32_t t = std::strtol(s, &end, 10);
  if (!end || end == s)
    return false;
  v = static_cast<int>(t);
  return true;
}

[[maybe_unused]] static inline void deg_to_ddmm_mmmm(double deg, char *out, size_t n, bool is_lon) {
  double a = std::fabs(deg);
  int d = static_cast<int>(a);
  double m = (a - d) * 60.0;
  if (is_lon) {
    std::snprintf(out, n, "%03d%07.4f", d, m);
  } else {
    std::snprintf(out, n, "%02d%07.4f", d, m);
  }
}

[[maybe_unused]] static bool parse_time_hhmmss(const char *s, int &hh, int &mm, int &ss) {
  if (!s || !*s)
    return false;
  // strip fractional part if any
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%s", s);
  char *dot = std::strchr(buf, '.');
  if (dot)
    *dot = '\0';
  size_t l = std::strlen(buf);
  if (l < 6)
    return false;
  hh = (buf[0] - '0') * 10 + (buf[1] - '0');
  mm = (buf[2] - '0') * 10 + (buf[3] - '0');
  ss = (buf[4] - '0') * 10 + (buf[5] - '0');
  return (hh >= 0 && hh < 24 && mm >= 0 && mm < 60 && ss >= 0 && ss < 60);
}

[[maybe_unused]] static bool parse_date_ddmmyy(const char *s, int &dd, int &mo, int &yy) {
  if (!s || std::strlen(s) < 6)
    return false;
  dd = (s[0] - '0') * 10 + (s[1] - '0');
  mo = (s[2] - '0') * 10 + (s[3] - '0');
  yy = (s[4] - '0') * 10 + (s[5] - '0');
  return (dd >= 1 && dd <= 31 && mo >= 1 && mo <= 12);
}

[[maybe_unused]] static int tokenize_csv(char *line, const char **tokens, int max_tokens) {
  int count = 0;
  if (line == nullptr || *line == '\0') {
    return 0;
  }
  tokens[count++] = line;
  while (count < max_tokens) {
    char *next_delim = strchr(line, ',');
    if (next_delim) {
      *next_delim = '\0';
      line = next_delim + 1;
      tokens[count++] = line;
    } else {
      break;
    }
  }
  return count;
}

static inline uint8_t nmea_checksum(const char *s) {
  uint8_t cs = 0;
  for (; *s; ++s)
    cs ^= static_cast<uint8_t>(*s);
  return cs;
}

static bool parse_cgnssinfo(const std::string &line, GnssInfo &gi) {
  const char *p = std::strchr(line.c_str(), ':');
  if (!p)
    return false;

  char buf[384];
  std::strncpy(buf, p + 1, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  const int MAXTOK = 32;
  const char *tok[MAXTOK] = {nullptr};
  int n = tokenize_csv(buf, tok, MAXTOK);

  auto T = [&](int idx) -> const char * { return (idx >= 0 && idx < n) ? tok[idx] : ""; };

  if (n >= 21) {
    // +CGNSINF (21 tokens)
    // <run>,<fix>,<UTC>,<lat>,<lon>,<msl_alt>,<spd_kmh>,<cog>,<fix_mode>,<rsv1>,<hdop>,<pdop>,<vdop>,...
    static constexpr double KMH_TO_KNOT = 0.539956803;

    (void) to_double(T(3), gi.lat_deg);
    (void) to_double(T(4), gi.lon_deg);
    (void) to_double(T(5), gi.alt_m);

    double spd_kmh = NAN;
    if (to_double(T(6), spd_kmh))
      gi.spd = spd_kmh * KMH_TO_KNOT;

    (void) to_double(T(7), gi.cog_deg);
    (void) to_double(T(10), gi.hdop);

    int used = 0;
    (void) to_int(T(15), used);
    gi.sat_used = used;

    const char *utc = T(2);
    if (utc && std::strlen(utc) >= 14) {
      int yyyy = (utc[0] - '0') * 1000 + (utc[1] - '0') * 100 + (utc[2] - '0') * 10 + (utc[3] - '0');
      gi.yy = yyyy % 100;
      gi.mo = (utc[4] - '0') * 10 + (utc[5] - '0');
      gi.dd = (utc[6] - '0') * 10 + (utc[7] - '0');
      gi.hh = (utc[8] - '0') * 10 + (utc[9] - '0');
      gi.mm = (utc[10] - '0') * 10 + (utc[11] - '0');
      gi.ss = (utc[12] - '0') * 10 + (utc[13] - '0');
    }

    int fix = 0;
    (void) to_int(T(1), fix);
    gi.fix_valid = (fix == 1) && std::isfinite(gi.lat_deg) && std::isfinite(gi.lon_deg);

  } else if (n == 18) {
    // +CGNSSINFO 18 tokens: lat/lon already in decimal degrees
    // mode, sat_used, fix_status, sat_view, fix_status_2, lat(dd), N/S, lon(dd), E/W, date, time, alt, spd, cog,
    // hdop, vdop, pdop, sat_view_2
    if (n < 15)
      return false;

    int sat_used = 0;
    (void) to_int(T(1), sat_used);
    gi.sat_used = sat_used;

    (void) to_double(T(5), gi.lat_deg);
    (void) to_double(T(7), gi.lon_deg);

    char lat_dir = T(6)[0] ? T(6)[0] : 'N';
    char lon_dir = T(8)[0] ? T(8)[0] : 'E';
    if (lat_dir == 'S')
      gi.lat_deg = -std::fabs(gi.lat_deg);
    if (lon_dir == 'W')
      gi.lon_deg = -std::fabs(gi.lon_deg);

    (void) to_double(T(11), gi.alt_m);
    (void) to_double(T(12), gi.spd);
    (void) to_double(T(13), gi.cog_deg);
    (void) to_double(T(14), gi.hdop);

    (void) parse_time_hhmmss(T(10), gi.hh, gi.mm, gi.ss);
    (void) parse_date_ddmmyy(T(9), gi.dd, gi.mo, gi.yy);

    int fix_status = 0;
    (void) to_int(T(2), fix_status);
    gi.fix_valid = (fix_status > 0) || (gi.sat_used > 0);

  } else if (n == 17) {
    // +CGNSSINFO 17 tokens: lat/lon in DDMM.MMMMMM
    // mode, sat_used, unknown, sat_view, fix_status, lat, N/S, lon, E/W, date, time, alt, spd, cog, hdop, vdop, pdop
    if (n < 15)
      return false;

    int sat_used = 0;
    (void) to_int(T(1), sat_used);
    gi.sat_used = sat_used;

    double lat_ddmm = NAN, lon_ddmm = NAN;
    (void) to_double(T(5), lat_ddmm);
    (void) to_double(T(7), lon_ddmm);

    int lat_d = static_cast<int>(lat_ddmm / 100.0);
    gi.lat_deg = lat_d + (lat_ddmm - lat_d * 100.0) / 60.0;
    int lon_d = static_cast<int>(lon_ddmm / 100.0);
    gi.lon_deg = lon_d + (lon_ddmm - lon_d * 100.0) / 60.0;

    char lat_dir = T(6)[0] ? T(6)[0] : 'N';
    char lon_dir = T(8)[0] ? T(8)[0] : 'E';
    if (lat_dir == 'S')
      gi.lat_deg = -gi.lat_deg;
    if (lon_dir == 'W')
      gi.lon_deg = -gi.lon_deg;

    (void) to_double(T(11), gi.alt_m);
    (void) to_double(T(12), gi.spd);
    (void) to_double(T(13), gi.cog_deg);
    (void) to_double(T(14), gi.hdop);

    (void) parse_time_hhmmss(T(10), gi.hh, gi.mm, gi.ss);
    (void) parse_date_ddmmyy(T(9), gi.dd, gi.mo, gi.yy);

    int fix_status = 0;
    (void) to_int(T(4), fix_status);
    gi.fix_valid = (fix_status > 0) || (gi.sat_used > 0);

  } else if (n == 16) {
    // +CGNSSINFO 16 tokens: lat/lon in DDMM.MMMMMM
    // mode, sat_used, sat_view, fix_status, lat, N/S, lon, E/W, date, time, alt, spd, cog, hdop, vdop, pdop
    if (n < 14)
      return false;

    int sat_used = 0;
    (void) to_int(T(1), sat_used);
    gi.sat_used = sat_used;

    double lat_ddmm = NAN, lon_ddmm = NAN;
    (void) to_double(T(4), lat_ddmm);
    (void) to_double(T(6), lon_ddmm);

    int lat_d = static_cast<int>(lat_ddmm / 100.0);
    gi.lat_deg = lat_d + (lat_ddmm - lat_d * 100.0) / 60.0;
    int lon_d = static_cast<int>(lon_ddmm / 100.0);
    gi.lon_deg = lon_d + (lon_ddmm - lon_d * 100.0) / 60.0;

    char lat_dir = T(5)[0] ? T(5)[0] : 'N';
    char lon_dir = T(7)[0] ? T(7)[0] : 'E';
    if (lat_dir == 'S')
      gi.lat_deg = -gi.lat_deg;
    if (lon_dir == 'W')
      gi.lon_deg = -gi.lon_deg;

    (void) to_double(T(10), gi.alt_m);
    (void) to_double(T(11), gi.spd);
    (void) to_double(T(12), gi.cog_deg);
    (void) to_double(T(13), gi.hdop);

    (void) parse_time_hhmmss(T(9), gi.hh, gi.mm, gi.ss);
    (void) parse_date_ddmmyy(T(8), gi.dd, gi.mo, gi.yy);

    int fix_status = 0;
    (void) to_int(T(3), fix_status);
    gi.fix_valid = (fix_status > 0) || (gi.sat_used > 0);

  } else {
    ESP_LOGW(TAG, "Unknown GNSS response format: %s (%d tokens)", line.c_str(), n);
    return false;
  }

  return std::isfinite(gi.lat_deg) && std::isfinite(gi.lon_deg);
}

bool ModemNMEAUARTComponent::read_array(uint8_t *data, size_t len) {
  if (available() < static_cast<int>(len))
    return false;
  std::memcpy(data, this->nmea_buffer_ + this->read_ptr_, len);
  this->read_ptr_ += len;
  return true;
}

void ModemNMEAUARTComponent::update() {
  if (!global_modem_component->modem_handler || !global_modem_component->modem_handler->dce ||
      global_modem_component->modem_handler->dce->sync() != esp_modem::command_result::OK)
    return;

  AtCommandResult gnss_command = global_modem_component->modem_handler->send_at(this->gnss_command_);
  if (!gnss_command.success) {
    ESP_LOGE(TAG, "Failed to execute GNSS command '%s'", this->gnss_command_.c_str());
    return;
  }
  std::string resp = gnss_command.output;
  ESP_LOGI(TAG, "GNSS command result: '%s'", resp.c_str());

  GnssInfo gi;
  if (!parse_cgnssinfo(resp, gi)) {
    ESP_LOGW(TAG, "Invalid GNSS data");
    return;
  }
  if (!gi.fix_valid || !std::isfinite(gi.hdop) || gi.sat_used <= 0) {
    ESP_LOGW(TAG, "GNSS not fixed");
    return;
  }

  char lat_ddmm[16], lon_ddmm[16];
  deg_to_ddmm_mmmm(gi.lat_deg, lat_ddmm, sizeof(lat_ddmm), false);
  deg_to_ddmm_mmmm(gi.lon_deg, lon_ddmm, sizeof(lon_ddmm), true);

  char lat_dir = gi.lat_deg >= 0 ? 'N' : 'S';
  char lon_dir = gi.lon_deg >= 0 ? 'E' : 'W';

  char time_str[11];
  std::snprintf(time_str, sizeof(time_str), "%02d%02d%02d.00", gi.hh, gi.mm, gi.ss);

  char date_str[7];
  std::snprintf(date_str, sizeof(date_str), "%02d%02d%02d", gi.dd, gi.mo, gi.yy);

  int fix_quality = gi.fix_valid ? 1 : 0;

  char gga[160];
  char sat_str[3];
  std::snprintf(sat_str, sizeof(sat_str), "%02d", std::min(gi.sat_used, 99));
  std::snprintf(gga, sizeof(gga), "GPGGA,%s,%s,%c,%s,%c,%d,%s,%.1f,%.1f,M,,M,,", time_str, lat_ddmm, lat_dir, lon_ddmm,
                lon_dir, fix_quality, sat_str, std::isfinite(gi.hdop) ? gi.hdop : 0.0,
                std::isfinite(gi.alt_m) ? gi.alt_m : 0.0);
  uint8_t cs_gga = nmea_checksum(gga);
  char full_gga[176];
  std::snprintf(full_gga, sizeof(full_gga), "$%s*%02X\r\n", gga, cs_gga);

  double spd_out = std::isfinite(gi.spd) ? gi.spd : 0.0;
  double cog_out = std::isfinite(gi.cog_deg) ? gi.cog_deg : 0.0;

  char rmc[160];
  std::snprintf(rmc, sizeof(rmc), "GPRMC,%s,A,%s,%c,%s,%c,%.1f,%.1f,%s,,,", time_str, lat_ddmm, lat_dir, lon_ddmm,
                lon_dir, spd_out, cog_out, date_str);
  uint8_t cs_rmc = nmea_checksum(rmc);
  char full_rmc[176];
  std::snprintf(full_rmc, sizeof(full_rmc), "$%s*%02X\r\n", rmc, cs_rmc);

  ESP_LOGI(TAG, "GPGGA: %s", full_gga);
  ESP_LOGI(TAG, "GPRMC: %s", full_rmc);

  size_t gga_len = std::strlen(full_gga);
  size_t rmc_len = std::strlen(full_rmc);
  size_t total = gga_len + rmc_len;

  if (total <= sizeof(this->nmea_buffer_)) {
    std::memcpy(this->nmea_buffer_, full_gga, gga_len);
    std::memcpy(this->nmea_buffer_ + gga_len, full_rmc, rmc_len);
    this->nmea_buffer_size_ = total;
    this->read_ptr_ = 0;
  } else {
    ESP_LOGW(TAG, "NMEA buffer too small (%u needed)", (unsigned) total);
  }
}

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF
