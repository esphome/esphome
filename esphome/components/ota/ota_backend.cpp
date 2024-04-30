#include "ota_backend.h"

namespace esphome {
namespace ota {

#ifdef USE_OTA_STATE_CALLBACK
OTAGlobalCallback *global_ota_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

OTAGlobalCallback::OTAGlobalCallback() { global_ota_component = this; }
#endif

}  // namespace ota
}  // namespace esphome
