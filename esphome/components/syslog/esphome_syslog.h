#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/udp/udp_component.h"
#include "esphome/components/time/real_time_clock.h"

#ifdef USE_NETWORK
namespace esphome::syslog {
enum SyslogFormat : uint8_t {
  SYSLOG_FORMAT_RFC3164,
  SYSLOG_FORMAT_RFC5424,
};

class Syslog final : public Component, public Parented<udp::UDPComponent> {
 public:
  Syslog(int level, time::RealTimeClock *time) : log_level_(level), time_(time) {}
  void setup() override;
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len);
  void set_strip(bool strip) { this->strip_ = strip; }
  void set_facility(int facility) { this->facility_ = facility; }
  void set_format(SyslogFormat format) { this->format_ = format; }

 protected:
  int log_level_;
  void log_(int level, const char *tag, const char *message, size_t message_len) const;
  time::RealTimeClock *time_;
  bool strip_{true};
  int facility_{16};
  SyslogFormat format_{SYSLOG_FORMAT_RFC3164};
};
}  // namespace esphome::syslog
#endif
