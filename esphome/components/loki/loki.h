#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/time/real_time_clock.h"

#ifdef USE_NETWORK
namespace esphome {
namespace loki {
class Loki : public Component, public Parented<http_request::HttpRequestComponent> {
 public:
  Loki(int level, time::RealTimeClock *time) : log_level_(level), time_(time) {}
  void setup() override;
  void set_strip(bool strip) { this->strip_ = strip; }

 protected:
  int log_level_;
  void log_(int level, const char *tag, const char *message, size_t message_len) const;
  time::RealTimeClock *time_;
  bool strip_{true};
};
}  // namespace loki
}  // namespace esphome
#endif
