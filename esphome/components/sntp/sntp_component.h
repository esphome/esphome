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

  void setup() override;
  void dump_config() override;

  /// Change the servers used by SNTP for timekeeping
  void set_servers(std::string server_1, std::string server_2, std::string server_3) {
    this->set_servers(std::vector<std::string>{
        std::move(server_1),
        std::move(server_2),
        std::move(server_3),
    });
  }
  void set_servers(std::vector<std::string> servers);
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;
#if defined(USE_ESP32)
  void set_update_interval(uint32_t update_interval) override;
  uint32_t get_update_interval() const override;
#endif

 protected:
  void setup_servers_();

 private:
  // Private because buffer address should stay unchanged
  std::vector<std::string> servers_;

 protected:
#if !defined(USE_ESP32)
  bool has_time_{false};
#endif
  bool servers_was_setup_{false};
};

}  // namespace sntp
}  // namespace esphome
