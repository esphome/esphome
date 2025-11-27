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
  SNTPComponent(const std::array<const char *, SNTP_SERVER_COUNT> &servers) : servers_(servers) {}

  void setup() override;
  void dump_config() override;

  void set_servers(const std::array<const char *, SNTP_SERVER_COUNT> &servers);
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;
#if defined(USE_ESP32)
  void set_update_interval(uint32_t update_interval) override;
  uint32_t get_update_interval() const override;
#endif

 protected:
  void setup_servers_();

  // Store const char pointers to string literals
  // ESP8266: strings in rodata (RAM), but avoids std::string overhead (~24 bytes each)
  // Other platforms: strings in flash
  std::array<const char *, SNTP_SERVER_COUNT> servers_;
#if !defined(USE_ESP32)
  bool has_time_{false};
#endif
  bool servers_was_setup_{false};
};

}  // namespace sntp
}  // namespace esphome
