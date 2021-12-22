#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/oralb_ble/oralb_ble.h"

#ifdef USE_ESP32

namespace esphome {
namespace oralb_brush {

class OralbBrush : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_state(sensor::Sensor *state) { state_ = state; }

 protected:
  uint64_t address_;
  sensor::Sensor *state_{nullptr};
};

}  // namespace oralb_brush
}  // namespace esphome

#endif  // USE_ESP32
