#include "loki.h"

#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include "esphome/core/time.h"

namespace esphome {
namespace loki {

void Loki::setup() {
  logger::global_logger->add_on_log_callback(
      [this](int level, const char *tag, const char *message, size_t message_len) {
        this->log_(level, tag, message, message_len);
      });
}

void Loki::log_(const int level, const char *tag, const char *message, size_t message_len) const {
  if (!this->enabled_ || level > this->log_level_)
    return;
  // Syslog PRI calculation: facility * 8 + severity
  int severity = 7;
  int pri = 0 * 8 + severity;
  auto timestamp = this->time_->now().strftime("%b %e %H:%M:%S");
  size_t len = message_len;
  // remove color formatting
  if (this->strip_ && message[0] == 0x1B && len > 11) {
    message += 7;
    len -= 11;
  }

  auto data = str_sprintf("<%d>%s %s %s: %.*s", pri, timestamp.c_str(), App.get_name().c_str(), tag, len, message);
  // TODO: Implement actual HTTP request to Loki endpoint
  // this->parent_->send_packet((const uint8_t *) data.data(), data.size());
}

}  // namespace loki
}  // namespace esphome
