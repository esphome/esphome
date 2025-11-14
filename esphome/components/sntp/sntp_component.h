#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sntp {

/// The SNTP component allows you to configure local timekeeping via Simple Network Time Protocol.
///
/// \note
/// The C library (newlib) available on ESPs only supports TZ strings that specify an offset and DST info;
/// you cannot specify zone names or paths to zoneinfo files.
/// \see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
class SNTPComponent : public time::RealTimeClock {
 public:
  SNTPComponent(std::initializer_list<const char *> servers) : servers_(servers) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;

  void time_synced();

 protected:
  // Store const char pointers to string literals (max 3 servers)
  // Uses FixedVector to avoid heap allocation but support runtime sizing
  // ESP8266: strings in rodata (RAM), but avoids std::string overhead (~24 bytes each)
  // Other platforms: strings in flash
  FixedVector<const char *> servers_;
  bool has_time_{false};

#if defined(USE_ESP32)
 private:
  static SNTPComponent *instance;
#endif
};

}  // namespace sntp
}  // namespace esphome
