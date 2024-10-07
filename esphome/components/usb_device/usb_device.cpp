#if defined(USE_ESP32_VARIANT_ESP32S2)
#include "usb_device.h"
#include "esphome/core/log.h"

#ifdef USE_ARDUINO
#include "USB.h"
#else
#include "esp32s2/rom/usb/usb_dc.h"
#endif

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

#ifndef USE_ARDUINO
bool usb_configured = false;
void status_callback(enum usb_dc_status_code cb_status, uint8_t *param) {
  ESP_LOGD(TAG, "USB dc status %d", cb_status);
  switch(cb_status) {
    case USB_DC_ERROR:
    case USB_DC_RESET:
    case USB_DC_DISCONNECTED:
      usb_configured = false;
      break;
    case USB_DC_CONFIGURED:
      usb_configured = true;
      break;
    default:
      break;
  }
}
#endif

void UsbDevice::update() {
#ifndef USE_ARDUINO
#endif
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
#ifdef USE_ARDUINO
  return USB;
#else
  return usb_configured;
#endif
  return false;
}

}  // namespace usb_device
}  // namespace esphome
#endif
