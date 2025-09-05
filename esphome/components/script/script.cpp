#include "script.h"
#include "esphome/core/log.h"

namespace esphome {
namespace script {

static const char *const TAG = "script";

#ifdef USE_STORE_LOG_STR_IN_FLASH
void ScriptLogger::esp_log_(int level, int line, const __FlashStringHelper *format, const char *param) {
  esp_log_printf_(level, TAG, line, format, param);
}
#else
void ScriptLogger::esp_log_(int level, int line, const char *format, const char *param) {
  esp_log_printf_(level, TAG, line, format, param);
}
#endif

}  // namespace script
}  // namespace esphome
