#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/udp/udp_component.h"
#include "esphome/components/time/real_time_clock.h"

#ifdef USE_NETWORK
namespace esphome::syslog {
class Syslog : public Component, public Parented<udp::UDPComponent> {
 public:
  Syslog(int level, time::RealTimeClock *time) : log_level_(level), time_(time) {}
  void setup() override;
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len);
  void set_strip(bool strip) { this->strip_ = strip; }
  void set_facility(int facility) { this->facility_ = facility; }
  void set_hostname(const std::string &hostname) { this->hostname_ = hostname; }

 protected:
  int log_level_;
  void log_(int level, const char *tag, const char *message, size_t message_len) const;
  time::RealTimeClock *time_;
  bool strip_{true};
  int facility_{16};
  std::string hostname_;
};
}  // namespace esphome::syslog
#endif
