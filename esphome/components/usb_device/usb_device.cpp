#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_device.h"
#include "esphome/core/log.h"
#include "USB.h"
// based on defines in HWCDC.cpp
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3
#if ARDUINO_USB_MODE
#include "HWCDC.h"
#endif
#endif

extern "C" {
bool configured;

void __wrap_tud_mount_cb(void){
  configured = true;
}

void __wrap_tud_umount_cb(void){
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
    configured_->publish_state(get_configured_());
  }
#endif
}

void UsbDevice::dump_config() {
  // bool configured = USB;
  ESP_LOGCONFIG(TAG, "USB device - configured: %s", YESNO(get_configured_()));
}

#ifdef USE_BINARY_SENSOR
void UsbDevice::set_configured_binary_sensor(binary_sensor::BinarySensor *sensor) { configured_ = sensor; };
#endif

bool UsbDevice::get_configured_() {
// based on defines in HWCDC.cpp
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3
#if ARDUINO_USB_MODE
#if ARDUINO_USB_CDC_ON_BOOT//Serial used for USB CDC
  return Serial;
#else
  return USBSerial;
#endif
#endif
#endif
  return false;
}


}  // namespace usb_device
}  // namespace esphome
#endif
