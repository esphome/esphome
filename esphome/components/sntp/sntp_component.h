#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

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
  SNTPComponent(const std::vector<std::string> &servers) : servers_(servers) {}

  // Note: set_servers() has been removed and replaced by a constructor - calling set_servers after setup would
  // have had no effect anyway, and making the strings immutable avoids the need to strdup their contents.

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;

 protected:
  std::vector<std::string> servers_;
  bool has_time_{false};
};

}  // namespace sntp
}  // namespace esphome
