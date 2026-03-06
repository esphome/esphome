#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include <ctime>

namespace esphome {
namespace nmea {

// Shared data structure for GNSS information
struct GnssInfo {
  double lat_deg = NAN;
  double lon_deg = NAN;
  double alt_m = NAN;
  double hdop = NAN;
  double cog_deg = NAN;
  double spd = NAN;  // knots (normalized)
  int sat_used = 0;
  bool fix_valid = false;
  time_t utc_time = 0;  // UTC time as Unix timestamp
};

class NMEAComponent : public uart::UARTComponent, public PollingComponent {
 public:
  void add_on_update_callback(std::function<void()> &&callback) { this->on_update_callback_.add(std::move(callback)); }

  // UART read/write methods
  void write_array(const uint8_t *data, size_t len) override{};
  bool peek_byte(uint8_t *data) override { return false; };
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return this->nmea_buffer_size_ - this->read_ptr_; }
  void flush() override {}
  void check_logger_conflict() override {}

  // Pure virtual - platforms must implement
  void update() override = 0;

 protected:
  // Helper for platforms to populate buffer from GnssInfo
  void populate_nmea_from_gnss_info_(const GnssInfo &gi);

  uint8_t nmea_buffer_[168] = {0};  // max nmea length is 82, we use 168 to accommodate both GPGGA and GPRMC
  size_t nmea_buffer_size_{0};
  size_t read_ptr_{0};

  CallbackManager<void()> on_update_callback_;
};

}  // namespace nmea
}  // namespace esphome
