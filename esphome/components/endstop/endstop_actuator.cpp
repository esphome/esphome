#include "endstop_actuator.h"
#include "esphome/core/log.h"

namespace esphome::endstop {

void log_endstop_reached(const char *tag, const char *name, bool open, uint32_t duration_ms) {
  ESP_LOGD(tag, "'%s' - %s endstop reached. Took %.1fs.", name,
           open ? LOG_STR_LITERAL("Open") : LOG_STR_LITERAL("Close"), duration_ms / 1e3f);
}

void log_max_duration_reached(const char *tag, const char *name) {
  ESP_LOGD(tag, "'%s' - Max duration reached. Stopping.", name);
}

}  // namespace esphome::endstop
