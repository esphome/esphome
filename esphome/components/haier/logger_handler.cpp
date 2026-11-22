#include "logger_handler.h"
#include "esphome/core/log.h"

namespace esphome::haier {

void esphome_logger(haier_protocol::HaierLogLevel level, const char *tag, const char *message) {
  int esp_level;
  switch (level) {
    case haier_protocol::HaierLogLevel::LEVEL_ERROR:
      esp_level = ESPHOME_LOG_LEVEL_ERROR;
      break;
    case haier_protocol::HaierLogLevel::LEVEL_WARNING:
      esp_level = ESPHOME_LOG_LEVEL_WARN;
      break;
    case haier_protocol::HaierLogLevel::LEVEL_INFO:
      esp_level = ESPHOME_LOG_LEVEL_INFO;
      break;
    case haier_protocol::HaierLogLevel::LEVEL_DEBUG:
      esp_level = ESPHOME_LOG_LEVEL_DEBUG;
      break;
    case haier_protocol::HaierLogLevel::LEVEL_VERBOSE:
      esp_level = ESPHOME_LOG_LEVEL_VERBOSE;
      break;
    default:
      // Just ignore everything else
      return;
  }
  esp_log_printf_(esp_level, tag, __LINE__, ESPHOME_LOG_FORMAT("%s"), message);
}

void init_haier_protocol_logging() { haier_protocol::set_log_handler(esphome::haier::esphome_logger); };

}  // namespace esphome::haier
