#include "reboot_hook_recorder.h"
#include "esphome/core/log.h"

namespace esphome::reboot_hook_recorder {

ESPHOME_LOG_TAG(TAG, "reboot_hook_recorder");

void RebootHookRecorder::record_(const char *hook) { ESP_LOGI(TAG, "HOOK %s %s", this->name_, hook); }

}  // namespace esphome::reboot_hook_recorder
