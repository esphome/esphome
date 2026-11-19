#include "factory_reset_device.h"

#include "esphome/core/defines.h"

#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::factory_reset {

ESPHOME_LOG_TAG(TAG, "factory_reset");

static void reset_and_reboot() {
  global_preferences->reset();
  App.safe_reboot();
}

void factory_reset_device() {
  ESP_LOGI(TAG, "Resetting");
  // Let MQTT settle a bit
  delay(100);  // NOLINT
#ifdef USE_OPENTHREAD
  openthread::global_openthread_component->on_factory_reset(reset_and_reboot);
#else
  reset_and_reboot();
#endif
}

}  // namespace esphome::factory_reset
