#include "esphome_syslog.h"

#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include "esphome/core/time.h"

namespace esphome {
namespace syslog {

// Map log levels to syslog severity using an array, indexed by ESPHome log level (1-7)
constexpr int LOG_LEVEL_TO_SYSLOG_SEVERITY[] = {
    3,  // NONE
    3,  // ERROR
    4,  // WARN
    5,  // INFO
    6,  // CONFIG
    7,  // DEBUG
    7,  // VERBOSE
    7   // VERY_VERBOSE
};

void Syslog::setup() {
  logger::global_logger->add_on_log_callback(
      [this](int level, const char *tag, const char *message) { this->log_(level, tag, message); });
}

void Syslog::log_(const int level, const char *tag, const char *message) const {
  if (level > this->log_level_)
    return;
  // Syslog PRI calculation: facility * 8 + severity
  int severity = 7;
  if ((unsigned) level <= 7) {
    severity = LOG_LEVEL_TO_SYSLOG_SEVERITY[level];
  }
  int pri = this->facility_ * 8 + severity;
  auto timestamp = this->time_->now().strftime("%b %d %H:%M:%S");
  unsigned len = strlen(message);
  // remove color formatting
  if (this->strip_ && message[0] == 0x1B && len > 11) {
    message += 7;
    len -= 11;
  }

  auto data = str_sprintf("<%d>%s %s %s: %.*s", pri, timestamp.c_str(), App.get_name().c_str(), tag, len, message);
  this->parent_->send_packet((const uint8_t *) data.data(), data.size());
}

}  // namespace syslog
}  // namespace esphome
