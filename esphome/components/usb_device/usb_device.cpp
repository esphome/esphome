#ifdef USE_ESP32_VARIANT_ESP32S2
#include "usb_device.h"
#include "esphome/core/log.h"
#include "Adafruit_TinyUSB.h"
#include "USB.h"

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

void UsbDevice::setup() { USB.begin(); }

void UsbDevice::update() {
#ifdef USE_BINARY_SENSOR
  if (mounted_ != nullptr) {
    mounted_->publish_state(TinyUSBDevice.mounted());
  }
  if (ready_ != nullptr) {
    ready_->publish_state(TinyUSBDevice.ready());
  }
  if (suspended_ != nullptr) {
    suspended_->publish_state(TinyUSBDevice.suspended());
  }
#endif
}

float UsbDevice::get_setup_priority() const {
  // it should be registered after all USB components
  return setup_priority::HARDWARE - 100;
}

void UsbDevice::dump_config() {
  ESP_LOGCONFIG(TAG, "USB device - mounted: %s, suspended: %s, ready: %s", YESNO(TinyUSBDevice.mounted()),
                YESNO(TinyUSBDevice.suspended()), YESNO(TinyUSBDevice.ready()));
}

#ifdef USE_BINARY_SENSOR
void UsbDevice::set_mounted_binary_sensor(binary_sensor::BinarySensor *sensor) { mounted_ = sensor; };
void UsbDevice::set_ready_binary_sensor(binary_sensor::BinarySensor *sensor) { ready_ = sensor; };
void UsbDevice::set_suspended_binary_sensor(binary_sensor::BinarySensor *sensor) { suspended_ = sensor; };
#endif

}  // namespace usb_device
}  // namespace esphome
#endif
