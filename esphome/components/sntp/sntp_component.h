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
  void setup() override;
  void dump_config() override;
  /// Change the servers used by SNTP for timekeeping
  void set_servers(const std::string &server_1, const std::string &server_2, const std::string &server_3) {
    this->server_1_ = server_1;
    this->server_2_ = server_2;
    this->server_3_ = server_3;
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void update() override;
  void loop() override;

 protected:
  std::string server_1_;
  std::string server_2_;
  std::string server_3_;
  bool has_time_{false};
};

}  // namespace sntp
}  // namespace esphome
