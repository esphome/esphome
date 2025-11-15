#if defined(USE_HOST)
#include "logger.h"

namespace esphome::logger {

void HOT Logger::write_msg_(const char *msg, size_t) {
  time_t rawtime;
  struct tm *timeinfo;
  char buffer[80];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, sizeof buffer, "[%H:%M:%S]", timeinfo);
  fputs(buffer, stdout);
  puts(msg);
}

void Logger::pre_setup() { global_logger = this; }

}  // namespace esphome::logger

#endif
