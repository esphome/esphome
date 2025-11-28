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

void OTAComponent::notify_state_(OTAState state, float progress, uint8_t error) {
  for (auto *listener : this->state_listeners_) {
    listener->on_ota_state(state, progress, error);
  }
  get_global_ota_callback()->notify_ota_state(state, progress, error, this);
}
#endif

}  // namespace ota
}  // namespace esphome
