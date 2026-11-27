#include "restart.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::restart {

ESPHOME_LOG_TAG(TAG, "restart");

void restart_device() {
  ESP_LOGI(TAG, "Restarting device");
  App.safe_reboot();
}

}  // namespace esphome::restart
