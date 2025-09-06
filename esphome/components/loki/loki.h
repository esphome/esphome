#pragma once
#include <string>
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
  void set_url(const std::string &url) { this->url_ = url; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_enabled(bool enabled) { this->enabled_ = enabled; }

  // Helper methods
  bool is_enabled() const { return this->enabled_; }
  void enable() { this->enabled_ = true; }
  void disable() { this->enabled_ = false; }
  std::string get_url() const { return this->url_; }
  uint16_t get_port() const { return this->port_; }
  int get_log_level() const { return this->log_level_; }
  bool is_strip_enabled() const { return this->strip_; }

 protected:
  int log_level_;
  void log_(int level, const char *tag, const char *message, size_t message_len) const;
  time::RealTimeClock *time_;
  bool strip_{true};
  bool enabled_{true};
  std::string url_;
  uint16_t port_{3100};
};
}  // namespace loki
}  // namespace esphome
#endif
