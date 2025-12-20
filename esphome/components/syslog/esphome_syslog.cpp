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

void Syslog::setup() { logger::global_logger->add_log_listener(this); }

void Syslog::on_log(uint8_t level, const char *tag, const char *message, size_t message_len) {
  this->log_(level, tag, message, message_len);
}

void Syslog::log_(const int level, const char *tag, const char *message, size_t message_len) const {
  if (level > this->log_level_)
    return;
  // Syslog PRI calculation: facility * 8 + severity
  int severity = 7;
  if ((unsigned) level <= 7) {
    severity = LOG_LEVEL_TO_SYSLOG_SEVERITY[level];
  }
  int pri = this->facility_ * 8 + severity;
  auto now = this->time_->now();
  std::string timestamp;
  if (now.is_valid()) {
    timestamp = now.strftime("%b %e %H:%M:%S");
  } else {
    // RFC 5424: A syslog application MUST use the NILVALUE as TIMESTAMP if the syslog application is incapable of
    //           obtaining system time.
    timestamp = "-";
  }
  size_t len = message_len;
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
