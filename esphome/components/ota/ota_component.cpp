#include "ota_component.h"

namespace esphome {
namespace ota {

OTAComponent *global_ota_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_OTA_STATE_CALLBACK
void OTAComponent::add_on_state_callback(std::function<void(OTAState, float, uint8_t)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
#endif

}  // namespace ota
}  // namespace esphome
