#pragma once

#include <ctime>
#include <string>

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_scanner {

class BLEScanner : public text_sensor::TextSensor, public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    this->publish_state("{\"timestamp\":" + to_string(::time(nullptr)) +
                        ","
                        "\"address\":\"" +
                        device.address_str() +
                        "\","
                        "\"rssi\":" +
                        to_string(device.get_rssi()) +
                        ","
                        "\"name\":\"" +
                        device.get_name() + "\"}");

    return true;
  }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace ble_scanner
}  // namespace esphome

#endif
