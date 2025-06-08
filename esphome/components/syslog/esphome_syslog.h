#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/udp/udp_component.h"
#include "esphome/components/time/real_time_clock.h"

#ifdef USE_NETWORK
namespace esphome {
namespace syslog {
class Syslog : public Component, public Parented<udp::UDPComponent> {
 public:
  Syslog(int level, time::RealTimeClock *time) : log_level_(level), time_(time) {}
  void setup() override;
  void set_strip(bool strip) { this->strip_ = strip; }
  void set_facility(int facility) { this->facility_ = facility; }

 protected:
  int log_level_;
  void log_(int level, const char *tag, const char *message) const;
  time::RealTimeClock *time_;
  bool strip_{true};
  int facility_{16};
};
}  // namespace syslog
}  // namespace esphome
#endif
