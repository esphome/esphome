#include "factory_reset_button.h"

#include "esphome/core/defines.h"

#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace factory_reset {

static const char *const TAG = "factory_reset.button";

void FactoryResetButton::dump_config() { LOG_BUTTON("", "Factory Reset Button", this); }
void FactoryResetButton::press_action() {
  ESP_LOGI(TAG, "Resetting");
  // Let MQTT settle a bit
  delay(100);  // NOLINT
#ifdef USE_OPENTHREAD
  openthread::global_openthread_component->on_factory_reset(FactoryResetButton::factory_reset_callback);
#else
  global_preferences->reset();
  App.safe_reboot();
#endif
}

#ifdef USE_OPENTHREAD
void FactoryResetButton::factory_reset_callback() {
  global_preferences->reset();
  App.safe_reboot();
}
#endif

}  // namespace factory_reset
}  // namespace esphome
