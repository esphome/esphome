#include "endstop_actuator.h"
#include "esphome/core/log.h"

namespace esphome::endstop {

void log_endstop_reached(const char *tag, const LogString *name, bool open, uint32_t duration_ms) {
  ESP_LOGD(tag, "'%s' - %s endstop reached. Took %.1fs.", LOG_STR_ARG(name),
           open ? LOG_STR_LITERAL("Open") : LOG_STR_LITERAL("Close"), duration_ms / 1e3f);
}

void log_max_duration_reached(const char *tag, const LogString *name) {
  ESP_LOGD(tag, "'%s' - Max duration reached. Stopping.", LOG_STR_ARG(name));
}

}  // namespace esphome::endstop
