#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include <array>

namespace esphome {
namespace sntp {

// Server count is calculated at compile time by Python codegen
// SNTP_SERVER_COUNT will always be defined

/// The SNTP component allows you to configure local timekeeping via Simple Network Time Protocol.
///
/// \note
/// The C library (newlib) available on ESPs only supports TZ strings that specify an offset and DST info;
/// you cannot specify zone names or paths to zoneinfo files.
/// \see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
class SNTPComponent : public time::RealTimeClock {
 public:
#if defined(USE_ESP32)
  SNTPComponent(const std::array<const char *, SNTP_SERVER_COUNT> &servers, bool smooth_sync)
      : servers_(servers), smooth_sync_(smooth_sync) {}
#else
  SNTPComponent(const std::array<const char *, SNTP_SERVER_COUNT> &servers, bool smooth_sync) : servers_(servers) {}
#endif
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;

  void time_synced();

 protected:
  // Store const char pointers to string literals
  // ESP8266: strings in rodata (RAM), but avoids std::string overhead (~24 bytes each)
  // Other platforms: strings in flash
  std::array<const char *, SNTP_SERVER_COUNT> servers_;
  bool has_time_{false};

#if defined(USE_ESP32)
  bool smooth_sync_;
  bool is_syncing_{false};
  uint32_t last_sync_status_check_{0};
#endif

#if defined(USE_ESP32)
 private:
  static SNTPComponent *instance;
#endif
};

}  // namespace sntp
}  // namespace esphome
