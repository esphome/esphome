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
  void set_servers(const std::string &server_1, const std::string &server_2, const std::string &server_3);
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;
#if defined(USE_ESP_IDF)
  void set_update_interval(uint32_t update_interval) override;
  uint32_t get_update_interval() const override;
#endif

 protected:
  void setup_servers_();

 private:
  // Private because buffer address should stay unchanged
  std::string servers_[3];

 protected:
#if !defined(USE_ESP_IDF)
  bool has_time_{false};
#endif
  bool servers_was_setup_{false};
};

}  // namespace sntp
}  // namespace esphome
