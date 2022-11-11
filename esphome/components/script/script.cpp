#include "script.h"
#include "esphome/core/log.h"

namespace esphome {
namespace script {

static const char *const TAG = "script";

void ScriptLogger::esp_log_(int level, int line, const char *format, const char *param) {
  esp_log_printf_(level, TAG, line, format, param);
}

}  // namespace script
}  // namespace esphome
