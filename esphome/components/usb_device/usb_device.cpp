#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_device.h"
#include "esphome/core/log.h"
#include "USB.h"


extern "C" {
bool configured;

void tud_mount_cb(void){
  configured = true;
}

void tud_umount_cb(void){
  configured = false;
}

}

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

void UsbDevice::update() {
#ifdef USE_BINARY_SENSOR
  if (configured_ != nullptr) {
    // bool configured = USB;
    configured_->publish_state(configured);
  }
#endif
}

void UsbDevice::dump_config() {
  // bool configured = USB;
  ESP_LOGCONFIG(TAG, "USB device - configured: %s", YESNO(configured));
}

#ifdef USE_BINARY_SENSOR
void UsbDevice::set_configured_binary_sensor(binary_sensor::BinarySensor *sensor) { configured_ = sensor; };
#endif

}  // namespace usb_device
}  // namespace esphome
#endif
