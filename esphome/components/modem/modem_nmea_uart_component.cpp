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

// single entry point, content selected by #ifdef to keep only what's used
static bool parse_cgnssinfo(const std::string &line, GnssInfo &gi) {
  const char *p = std::strchr(line.c_str(), ':');
  if (!p)
    return false;
  [[maybe_unused]] const char *start = p + 1;

#ifdef USE_MODEM_GNSS_PARSER_CGNSSINFO16
  // 16 tokens: mode, sat_used, sat_view, fix_status, lat, N/S, lon, E/W, date, time, alt, spd, cog, hdop, vdop, pdop
  char buf[256];
  std::strncpy(buf, start, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  const int EXPECT = 16;
  const char *tok[EXPECT] = {nullptr};
  int i = tokenize_csv(buf, tok, EXPECT);
  if (i < 14)
    return false;  // need up to hdop at least

  int sat_used = 0;
  (void) to_int(tok[1], sat_used);
  gi.sat_used = sat_used;

  // lat/lon are DDMM.MMMMMM here
  double lat_ddmm = NAN, lon_ddmm = NAN;
  (void) to_double(tok[4], lat_ddmm);
  (void) to_double(tok[6], lon_ddmm);

  int lat_deg = static_cast<int>(lat_ddmm / 100.0);
  double lat_min = lat_ddmm - lat_deg * 100.0;
  gi.lat_deg = lat_deg + lat_min / 60.0;
  int lon_deg = static_cast<int>(lon_ddmm / 100.0);
  double lon_min = lon_ddmm - lon_deg * 100.0;
  gi.lon_deg = lon_deg + lon_min / 60.0;

  char lat_dir = (tok[5] && *tok[5]) ? tok[5][0] : 'N';
  char lon_dir = (tok[7] && *tok[7]) ? tok[7][0] : 'E';
  if (lat_dir == 'S')
    gi.lat_deg = -gi.lat_deg;
  if (lon_dir == 'W')
    gi.lon_deg = -gi.lon_deg;

  (void) to_double(tok[10], gi.alt_m);
  (void) to_double(tok[11], gi.spd);
  (void) to_double(tok[12], gi.cog_deg);
  (void) to_double(tok[13], gi.hdop);

  (void) parse_time_hhmmss(tok[9], gi.hh, gi.mm, gi.ss);
  (void) parse_date_ddmmyy(tok[8], gi.dd, gi.mo, gi.yy);

  int fix_status = 0;
  (void) to_int(tok[3], fix_status);
  gi.fix_valid = (fix_status > 0) || (gi.sat_used > 0);

  return std::isfinite(gi.lat_deg) && std::isfinite(gi.lon_deg);
#endif

#ifdef USE_MODEM_GNSS_PARSER_CGNSSINFO17
  // 17 tokens: mode, sat_used, unknown, sat_view, fix_status, lat, N/S, lon, E/W, date, time, alt, spd, cog, hdop,
  // vdop, pdop
  char buf[256];
  std::strncpy(buf, start, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  const int EXPECT = 17;
  const char *tok[EXPECT] = {nullptr};
  int i = tokenize_csv(buf, tok, EXPECT);
  if (i < 15)
    return false;  // need up to hdop at least

  int sat_used = 0;
  (void) to_int(tok[1], sat_used);
  gi.sat_used = sat_used;

  // lat/lon are DDMM.MMMMMM here
  double lat_ddmm = NAN, lon_ddmm = NAN;
  (void) to_double(tok[5], lat_ddmm);
  (void) to_double(tok[7], lon_ddmm);

  int lat_deg = static_cast<int>(lat_ddmm / 100.0);
  double lat_min = lat_ddmm - lat_deg * 100.0;
  gi.lat_deg = lat_deg + lat_min / 60.0;
  int lon_deg = static_cast<int>(lon_ddmm / 100.0);
  double lon_min = lon_ddmm - lon_deg * 100.0;
  gi.lon_deg = lon_deg + lon_min / 60.0;

  char lat_dir = (tok[6] && *tok[6]) ? tok[6][0] : 'N';
  char lon_dir = (tok[8] && *tok[8]) ? tok[8][0] : 'E';
  if (lat_dir == 'S')
    gi.lat_deg = -gi.lat_deg;
  if (lon_dir == 'W')
    gi.lon_deg = -gi.lon_deg;

  (void) to_double(tok[11], gi.alt_m);
  (void) to_double(tok[12], gi.spd);
  (void) to_double(tok[13], gi.cog_deg);
  (void) to_double(tok[14], gi.hdop);

  (void) parse_time_hhmmss(tok[10], gi.hh, gi.mm, gi.ss);
  (void) parse_date_ddmmyy(tok[9], gi.dd, gi.mo, gi.yy);

  int fix_status = 0;
  (void) to_int(tok[4], fix_status);
  gi.fix_valid = (fix_status > 0) || (gi.sat_used > 0);

  return std::isfinite(gi.lat_deg) && std::isfinite(gi.lon_deg);
#endif

#ifdef USE_MODEM_GNSS_PARSER_CGNSSINFO18
  // 18 tokens: mode, sat_used, fix_status, sat_view, fix_status_2, lat(dd), N/S, lon(dd), E/W, date, time, alt, spd,
  // cog, hdop, vdop, pdop, sat_view_2
  char buf[384];
  std::strncpy(buf, start, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  const int EXPECT = 18;
  const char *tok[EXPECT] = {nullptr};
  int i = tokenize_csv(buf, tok, EXPECT);
  if (i < 15)
    return false;  // need up to hdop at least

  int sat_used = 0;
  (void) to_int(tok[1], sat_used);
  gi.sat_used = sat_used;

  // lat/lon already in decimal degrees here
  (void) to_double(tok[5], gi.lat_deg);
  (void) to_double(tok[7], gi.lon_deg);

  char lat_dir = (tok[6] && *tok[6]) ? tok[6][0] : 'N';
  char lon_dir = (tok[8] && *tok[8]) ? tok[8][0] : 'E';
  if (lat_dir == 'S')
    gi.lat_deg = -std::fabs(gi.lat_deg);
  if (lon_dir == 'W')
    gi.lon_deg = -std::fabs(gi.lon_deg);

  (void) to_double(tok[11], gi.alt_m);
  (void) to_double(tok[12], gi.spd);
  (void) to_double(tok[13], gi.cog_deg);
  (void) to_double(tok[14], gi.hdop);

  (void) parse_time_hhmmss(tok[10], gi.hh, gi.mm, gi.ss);
  (void) parse_date_ddmmyy(tok[9], gi.dd, gi.mo, gi.yy);

  int fix_status = 0;
  (void) to_int(tok[2], fix_status);
  gi.fix_valid = (fix_status > 0) || (gi.sat_used > 0);

  return std::isfinite(gi.lat_deg) && std::isfinite(gi.lon_deg);
#endif

#ifdef USE_MODEM_GNSS_PARSER_CGNSINF21
  // +CGNSINF:
  // <run>,<fix>,<UTC>,<lat>,<lon>,<msl_alt>,<spd_kmh>,<cog>,<fix_mode>,<rsv1>,<hdop>,<pdop>,<vdop>,<rsv2>,<sv_view>,<sv_used>,<glo_view>,<rsv3>,<cn0_max>,<hpa>,<vpa>
  static constexpr double KMH_TO_KNOT = 0.539956803;

  char buf[384];
  std::strncpy(buf, start, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  const int MAXTOK = 32;
  const char *tok[MAXTOK] = {nullptr};
  int n = tokenize_csv(buf, tok, MAXTOK);

  auto S = [&](int idx) -> const char * { return (idx >= 1 && idx <= n) ? tok[idx - 1] : ""; };

  // lat/lon degrees
  (void) to_double(S(4), gi.lat_deg);
  (void) to_double(S(5), gi.lon_deg);

  // altitude
  (void) to_double(S(6), gi.alt_m);

  // speed: km/h → knots
  double spd_kmh = NAN;
  if (to_double(S(7), spd_kmh))
    gi.spd = spd_kmh * KMH_TO_KNOT;

  // course
  (void) to_double(S(8), gi.cog_deg);

  // hdop
  (void) to_double(S(11), gi.hdop);

  // sats used (16), or 0
  int used = 0;
  (void) to_int(S(16), used);
  gi.sat_used = used;

  // time/date if provided (3)
  // format UTC: yyyyMMddhhmmss.sss
  const char *utc = S(3);
  if (utc && std::strlen(utc) >= 14) {
    // date
    int yyyy = (utc[0] - '0') * 1000 + (utc[1] - '0') * 100 + (utc[2] - '0') * 10 + (utc[3] - '0');
    gi.yy = (yyyy % 100);
    gi.mo = (utc[4] - '0') * 10 + (utc[5] - '0');
    gi.dd = (utc[6] - '0') * 10 + (utc[7] - '0');
    // time
    gi.hh = (utc[8] - '0') * 10 + (utc[9] - '0');
    gi.mm = (utc[10] - '0') * 10 + (utc[11] - '0');
    gi.ss = (utc[12] - '0') * 10 + (utc[13] - '0');
  }
  // fix status (2): 1 = valid
  int fix = 0;
  (void) to_int(S(2), fix);
  gi.fix_valid = (fix == 1) && std::isfinite(gi.lat_deg) && std::isfinite(gi.lon_deg);

  return std::isfinite(gi.lat_deg) && std::isfinite(gi.lon_deg);
#endif

  return false;
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
