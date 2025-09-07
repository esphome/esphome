#pragma once
#include <string>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/time/real_time_clock.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

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
  void set_enabled(bool enabled) {
    this->enabled_ = enabled;
#ifdef USE_SWITCH
    if (this->logs_enabled_switch_ != nullptr) {
      this->logs_enabled_switch_->publish_state(enabled);
    }
#endif
  }

  // Helper methods
  bool is_enabled() const { return this->enabled_; }
  void enable() { this->set_enabled(true); }
  void disable() { this->set_enabled(false); }
  std::string get_url() const { return this->url_; }
  uint16_t get_port() const { return this->port_; }
  int get_log_level() const { return this->log_level_; }
  bool is_strip_enabled() const { return this->strip_; }

#ifdef USE_SWITCH
  // Switch methods
  void set_logs_enabled_switch(switch_::Switch *switch_) { this->logs_enabled_switch_ = switch_; }
#endif

 protected:
  int log_level_;
  void log_(int level, const char *tag, const char *message, size_t message_len) const;
  const char *get_log_level_name_(int level) const;
  time::RealTimeClock *time_;
  bool strip_{true};
  bool enabled_{true};
  std::string url_;
  uint16_t port_{3100};
#ifdef USE_SWITCH
  switch_::Switch *logs_enabled_switch_{nullptr};
#endif
};
}  // namespace loki
}  // namespace esphome
#endif
