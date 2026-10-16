#include "line_reader.h"
#include "esphome/core/log.h"

namespace esphome::uart {

void log_line_dropped(const char *tag, size_t max_len) {
  ESP_LOGW(tag, "Line over %u chars dropped", static_cast<unsigned>(max_len));
}

}  // namespace esphome::uart
