#pragma once
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/core/defines.h"
#ifdef USE_USB_CDC
#include <Adafruit_TinyUSB.h>
#include "USB.h"

namespace esphome {
namespace usb_device {

extern Adafruit_USBD_CDC *global_usb_serial;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace usb_device
}  // namespace esphome
#endif
#endif
