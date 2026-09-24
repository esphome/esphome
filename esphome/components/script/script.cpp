#include "script.h"
#include "esphome/core/log.h"

namespace esphome::script {

static const char *const TAG = "script";

void ScriptLogger::esp_log_(int level, int line, ProgmemStr format, const char *param) {
  esp_log_printf_(level, TAG, line, format, param);
}

}  // namespace esphome::script
