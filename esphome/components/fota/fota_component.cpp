#include "fota_component.h"
#include <zephyr/usb/usb_device.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

extern "C" void start_smp_bluetooth_adverts();

namespace esphome {
namespace fota {

void FOTAComponent::setup() {
  if (IS_ENABLED(CONFIG_USB_DEVICE_STACK)) {
    usb_enable(NULL);
  }
  start_smp_bluetooth_adverts();
}

}  // namespace fota
}  // namespace esphome
