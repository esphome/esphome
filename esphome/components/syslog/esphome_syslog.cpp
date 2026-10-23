#include "esphome_syslog.h"

#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/time.h"

namespace esphome::syslog {

// Map log levels to syslog severity using an array, indexed by ESPHome log level (1-7)
static constexpr uint8_t LOG_LEVEL_TO_SYSLOG_SEVERITY[] PROGMEM = {
    3,  // NONE
    3,  // ERROR
    4,  // WARN
    5,  // INFO
    6,  // CONFIG
    7,  // DEBUG
    7,  // VERBOSE
    7   // VERY_VERBOSE
};

static constexpr char SECRET_BEGIN[] = ESPHOME_LOG_SECRET_BEGIN;
static constexpr char SECRET_END[] = ESPHOME_LOG_SECRET_END;
static constexpr size_t SECRET_BEGIN_LEN = sizeof(SECRET_BEGIN) - 1;
static constexpr size_t SECRET_END_LEN = sizeof(SECRET_END) - 1;

static bool starts_with_marker(const char *message, size_t remaining, const char *marker, size_t marker_len) {
  return remaining >= marker_len && memcmp(message, marker, marker_len) == 0;
}

static size_t append_bytes(char *buf, size_t size, size_t pos, const char *data, size_t len) {
  if (pos >= size) {
    return size;
  }
  size_t n = std::min(len, size - pos - 1);
  memcpy(buf + pos, data, n);
  pos += n;
  buf[pos] = '\0';
  return pos;
}

size_t append_redacted_message(char *buf, size_t size, size_t pos, const char *message, size_t len) {
  size_t i = 0;
  while (i < len) {
    if (starts_with_marker(message + i, len - i, SECRET_BEGIN, SECRET_BEGIN_LEN)) {
      pos = buf_append_str(buf, size, pos, "[redacted]");
      i += SECRET_BEGIN_LEN;
      while (i < len && !starts_with_marker(message + i, len - i, SECRET_END, SECRET_END_LEN)) {
        i++;
      }
      if (i < len) {
        i += SECRET_END_LEN;
      }
      continue;
    }
    size_t start = i;
    while (i < len && !starts_with_marker(message + i, len - i, SECRET_BEGIN, SECRET_BEGIN_LEN)) {
      i++;
    }
    pos = append_bytes(buf, size, pos, message + start, i - start);
  }
  return pos;
}

void Syslog::setup() {
  logger::global_logger->add_log_callback(
      this, [](void *self, uint8_t level, const char *tag, const char *message, size_t message_len) {
        static_cast<Syslog *>(self)->on_log(level, tag, message, message_len);
      });
}

void Syslog::on_log(uint8_t level, const char *tag, const char *message, size_t message_len) {
  this->log_(level, tag, message, message_len);
}

void Syslog::log_(const int level, const char *tag, const char *message, size_t message_len) const {
  if (level > this->log_level_)
    return;
  // Syslog PRI calculation: facility * 8 + severity
  int severity = 7;
  if ((unsigned) level <= 7) {
    severity = progmem_read_byte(&LOG_LEVEL_TO_SYSLOG_SEVERITY[level]);
  }
  int pri = this->facility_ * 8 + severity;

  size_t len = message_len;
  // remove color formatting
  if (this->strip_ && message[0] == 0x1B && len > 11) {
    message += 7;
    len -= 11;
  }

  // Build syslog packet on stack (508 bytes chosen as practical limit for syslog over UDP)
  char packet[508];
  size_t offset = 0;
  // Write PRI - abort if this fails as packet would be malformed
  offset = buf_append_printf(packet, sizeof(packet), 0, "<%d>", pri);
  if (offset == 0) {
    return;  // PRI always produces at least "<0>" (3 chars), so 0 means error
  }
  auto now = this->time_->now();
  if (this->format_ == SYSLOG_FORMAT_RFC5424) {
    offset = buf_append_str(packet, sizeof(packet), offset, "1 ");

    char timestamp[32];
    size_t timestamp_len = now.is_valid() ? now.strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z") : 0;
    if (timestamp_len == 24) {
      // ESPTime formats the numeric offset as +HHMM. RFC 3339 requires +HH:MM.
      timestamp[25] = '\0';
      timestamp[24] = timestamp[23];
      timestamp[23] = timestamp[22];
      timestamp[22] = ':';
      offset = buf_append_printf(packet, sizeof(packet), offset, "%s", timestamp);
    } else {
      offset = buf_append_str(packet, sizeof(packet), offset, "-");
    }
    offset = buf_append_printf(packet, sizeof(packet), offset, " %s %s - - - ", App.get_name().c_str(), tag);
  } else {
    // RFC 3164 has no NILVALUE. If the clock is invalid, omit TIMESTAMP so a relay can add it.
    if (now.is_valid()) {
      offset += now.strftime(packet + offset, sizeof(packet) - offset, "%b %e %H:%M:%S ");
    }
    offset = buf_append_printf(packet, sizeof(packet), offset, "%s %s: ", App.get_name().c_str(), tag);
  }
  offset = append_redacted_message(packet, sizeof(packet), offset, message, len);
  // Clamp to exclude null terminator position if buffer was filled
  if (offset >= sizeof(packet)) {
    offset = sizeof(packet) - 1;
  }

  if (offset > 0) {
    this->parent_->send_packet(reinterpret_cast<const uint8_t *>(packet), offset);
  }
}

}  // namespace esphome::syslog
