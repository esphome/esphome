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

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

void UsbDevice::update() {
#ifdef USE_BINARY_SENSOR
  if (configured_ != nullptr) {
    configured_->publish_state(get_configured_());
  }
#endif
}

void UsbDevice::dump_config() {
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
#else
// this is subject of change by other components so make sure that we won't fail to report silently
#error Not implemented
#endif
#elif USE_ESP32_VARIANT_ESP32S2
  return USB;
#else
// this is subject of change by other components so make sure that we won't fail to report silently
#error Not implemented
#endif
  return false;
}


}  // namespace usb_device
}  // namespace esphome
#endif
