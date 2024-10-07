#if defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_device.h"
#include "esphome/core/log.h"

#ifdef USE_ARDUINO
#if ARDUINO_USB_MODE
#include "HWCDC.h"
#endif
#else
#include "esp32s3/rom/usb/usb_dc.h"
#endif

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

usb_dc_status_code g_cb_status;
void status_callback(enum usb_dc_status_code cb_status, uint8_t *param) {
  g_cb_status = cb_status;
}

void UsbDevice::update() {
  ESP_LOGD(TAG, "update %d", g_cb_status);
#ifdef USE_BINARY_SENSOR
  if (configured_ != nullptr) {
    configured_->publish_state(get_configured_());
  }
#endif
}

void UsbDevice::setup() {
#ifndef USE_ARDUINO
  usb_dc_set_status_callback(status_callback);
#endif
}

void UsbDevice::dump_config() { ESP_LOGCONFIG(TAG, "USB device - configured: %s", YESNO(get_configured_())); }

#ifdef USE_BINARY_SENSOR
void UsbDevice::set_configured_binary_sensor(binary_sensor::BinarySensor *sensor) { configured_ = sensor; };
#endif

bool UsbDevice::get_configured_() {
// ESP32:
// - Arduino framework does not support USB_SERIAL_JTAG.
// ESP32-S3
// - Arduino CDC logger is based on HWCDC.

#ifdef USE_ARDUINO
// based on defines in HWCDC.cpp
#if ARDUINO_USB_MODE
#if ARDUINO_USB_CDC_ON_BOOT  // Serial used for USB CDC
  return Serial;
#else
  return USBSerial;
#endif
#else
// this is subject of change by other components so make sure that we won't fail to report silently
#error Not implemented
#endif
#else

#endif
  return false;
}

}  // namespace usb_device
}  // namespace esphome
#endif
