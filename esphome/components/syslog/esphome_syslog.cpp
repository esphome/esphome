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

  size_t len = message_len;
  // remove color formatting
  if (this->strip_ && message[0] == 0x1B && len > 11) {
    message += 7;
    len -= 11;
  }

  // Build syslog packet on stack - 508 is max UDP packet size
  char packet[508];
  size_t offset = 0;

  // Write PRI
  int ret = snprintf(packet, sizeof(packet), "<%d>", pri);
  if (ret > 0)
    offset = ret;

  // Write timestamp directly into packet (RFC 5424: use "-" if time not valid)
  auto now = this->time_->now();
  if (now.is_valid()) {
    offset += now.strftime(packet + offset, sizeof(packet) - offset, "%b %e %H:%M:%S");
  } else {
    packet[offset++] = '-';
  }

  // Write hostname, tag, and message
  ret = snprintf(packet + offset, sizeof(packet) - offset, " %s %s: %.*s", App.get_name().c_str(), tag, (int) len,
                 message);
  if (ret > 0)
    offset += ret;

  if (offset > 0) {
    this->parent_->send_packet(reinterpret_cast<const uint8_t *>(packet), std::min(offset, sizeof(packet) - 1));
  }
}

}  // namespace syslog
}  // namespace esphome
