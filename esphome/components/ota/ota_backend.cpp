#include "ota_backend.h"

namespace esphome {
namespace ota {

#ifdef USE_OTA_STATE_LISTENER
OTAGlobalCallback *global_ota_callback{nullptr};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

OTAGlobalCallback *get_global_ota_callback() {
  if (global_ota_callback == nullptr) {
    global_ota_callback = new OTAGlobalCallback();  // NOLINT(cppcoreguidelines-owning-memory)
  }
  return global_ota_callback;
}

void register_ota_platform(OTAComponent *ota_caller) { get_global_ota_callback()->register_ota(ota_caller); }

void OTAComponentBridge::on_ota_state(OTAState state, float progress, uint8_t error) {
  this->global_callback_->notify_global_listeners(state, progress, error, this->component_);
}
#endif

}  // namespace ota
}  // namespace esphome
