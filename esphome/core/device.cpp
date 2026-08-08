#include "esphome/core/device.h"

#include "esphome/core/defines.h"
#if defined(USE_CONTROLLER_REGISTRY) && defined(USE_DEVICES)
#include "esphome/core/controller_registry.h"
#endif

namespace esphome {

void Device::set_available(bool available) {
  if (this->available_ == available) {
    return;
  }
  this->available_ = available;
#if defined(USE_CONTROLLER_REGISTRY) && defined(USE_DEVICES)
  ControllerRegistry::notify_device_update(this);
#endif
}

}  // namespace esphome
