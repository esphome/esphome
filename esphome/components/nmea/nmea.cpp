#include "nmea.h"
#include "esphome/core/log.h"
#include <cstring>
#include <cmath>

namespace esphome {
namespace nmea {

static const char *const TAG = "nmea";

static inline void deg_to_ddmm_mmmm(double deg, char *out, size_t n, bool is_lon) {
  double a = std::fabs(deg);
  int d = static_cast<int>(a);
  double m = (a - d) * 60.0;
  if (is_lon) {
    std::snprintf(out, n, "%03d%07.4f", d, m);
  } else {
    std::snprintf(out, n, "%02d%07.4f", d, m);
  }
}

static inline uint8_t nmea_checksum(const char *s) {
  uint8_t cs = 0;
  for (; *s; ++s)
    cs ^= static_cast<uint8_t>(*s);
  return cs;
}

bool NMEAComponent::read_array(uint8_t *data, size_t len) {
  if (available() < len)
    return false;
  std::memcpy(data, this->nmea_buffer_ + this->read_ptr_, len);
  this->read_ptr_ += len;
  return true;
}

void NMEAComponent::populate_nmea_from_gnss_info_(const GnssInfo &gi) {
  if (!gi.fix_valid || !std::isfinite(gi.hdop) || gi.sat_used <= 0) {
    ESP_LOGW(TAG, "GNSS not fixed, skipping NMEA generation");
    return;
  }

  char lat_ddmm[16], lon_ddmm[16];
  deg_to_ddmm_mmmm(gi.lat_deg, lat_ddmm, sizeof(lat_ddmm), false);
  deg_to_ddmm_mmmm(gi.lon_deg, lon_ddmm, sizeof(lon_ddmm), true);

  char lat_dir = gi.lat_deg >= 0 ? 'N' : 'S';
  char lon_dir = gi.lon_deg >= 0 ? 'E' : 'W';

  // Extract time/date from time_t (UTC)
  struct tm timeinfo;
  gmtime_r(&gi.utc_time, &timeinfo);

  char time_str[11];
  std::snprintf(time_str, sizeof(time_str), "%02d%02d%02d.00", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  char date_str[7];
  std::snprintf(date_str, sizeof(date_str), "%02d%02d%02d", timeinfo.tm_mday, timeinfo.tm_mon + 1,
                timeinfo.tm_year % 100);

  int fix_quality = gi.fix_valid ? 1 : 0;

  // Generate GPGGA sentence
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

  // Generate GPRMC sentence
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

    // Trigger the on_update callback
    this->on_update_callback_.call();
  } else {
    ESP_LOGW(TAG, "NMEA buffer too small (%u needed)", (unsigned) total);
  }
}

}  // namespace nmea
}  // namespace esphome
