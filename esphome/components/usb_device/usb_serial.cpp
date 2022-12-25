#include "usb_serial.h"
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#ifdef USE_USB_CDC
namespace esphome {
namespace usb_device {

Adafruit_USBD_CDC *global_usb_serial = new Adafruit_USBD_CDC(0);

}  // namespace usb_device
}  // namespace esphome
#endif
#endif
