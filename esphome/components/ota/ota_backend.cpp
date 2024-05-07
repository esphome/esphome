#include "ota_backend.h"

namespace esphome {
namespace ota {

#ifdef USE_OTA_STATE_CALLBACK
OTAGlobalCallback *global_ota_callback{nullptr};

OTAGlobalCallback *get_global_ota_callback() {
  if (global_ota_callback == nullptr) {
    global_ota_callback = new OTAGlobalCallback();
  }
  return global_ota_callback;
}

void register_ota_platform(OTAComponent *ota_caller) { get_global_ota_callback()->register_ota(ota_caller); }
#endif

}  // namespace ota
}  // namespace esphome
