#pragma once
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
namespace esphome {
namespace usb_device {

class UsbDevice : public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
#ifdef USE_BINARY_SENSOR
  void set_configured_binary_sensor(binary_sensor::BinarySensor *sensor);
#endif
 protected:
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *configured_;
#endif
  bool get_configured_();
};

}  // namespace usb_device
}  // namespace esphome
#endif
