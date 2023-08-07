#include "logger_handler.h"
#include "esphome/core/log.h"

namespace esphome {
namespace haier {

void esphome_logger(haier_protocol::HaierLogLevel level, const char *tag, const char *message) {
  switch (level) {
    case haier_protocol::HaierLogLevel::LEVEL_ERROR:
      esp_log_printf_(ESPHOME_LOG_LEVEL_ERROR, tag, __LINE__, "%s", message);
      break;
    case haier_protocol::HaierLogLevel::LEVEL_WARNING:
      esp_log_printf_(ESPHOME_LOG_LEVEL_WARN, tag, __LINE__, "%s", message);
      break;
    case haier_protocol::HaierLogLevel::LEVEL_INFO:
      esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, "%s", message);
      break;
    case haier_protocol::HaierLogLevel::LEVEL_DEBUG:
      esp_log_printf_(ESPHOME_LOG_LEVEL_DEBUG, tag, __LINE__, "%s", message);
      break;
    case haier_protocol::HaierLogLevel::LEVEL_VERBOSE:
      esp_log_printf_(ESPHOME_LOG_LEVEL_VERBOSE, tag, __LINE__, "%s", message);
      break;
    default:
      // Just ignore everything else
      break;
  }
}

void init_haier_protocol_logging() { haier_protocol::set_log_handler(esphome::haier::esphome_logger); };

}  // namespace haier
}  // namespace esphome
