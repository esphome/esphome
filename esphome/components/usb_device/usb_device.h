#pragma once
#ifdef USE_ESP32_VARIANT_ESP32S2
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
namespace esphome {
namespace usb_device {

class UsbDevice : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;
#ifdef USE_BINARY_SENSOR
  void set_mounted_binary_sensor(binary_sensor::BinarySensor *sensor);
  void set_ready_binary_sensor(binary_sensor::BinarySensor *sensor);
  void set_suspended_binary_sensor(binary_sensor::BinarySensor *sensor);
#endif
 protected:
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *mounted_;
  binary_sensor::BinarySensor *ready_;
  binary_sensor::BinarySensor *suspended_;
#endif
};

}  // namespace usb_device
}  // namespace esphome
#endif
