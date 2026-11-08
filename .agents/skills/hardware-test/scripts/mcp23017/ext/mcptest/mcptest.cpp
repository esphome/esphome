#include "mcptest.h"
#include <cstdarg>
#include <cstdio>

namespace esphome::mcptest {

void log_line(int level, const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (level == ESPHOME_LOG_LEVEL_ERROR) {
    ESP_LOGE(TAG, "%s", buf);
  } else {
    ESP_LOGI(TAG, "%s", buf);
  }
}

}  // namespace esphome::mcptest
