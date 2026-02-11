#pragma once

#include <cinttypes>
#include <cstdio>
#include <ctime>

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_scanner {

class BLEScanner : public text_sensor::TextSensor, public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    // Format JSON using stack buffer to avoid heap allocations from string concatenation
    char buf[128];
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    snprintf(buf, sizeof(buf), "{\"timestamp\":%" PRId64 ",\"address\":\"%s\",\"rssi\":%d,\"name\":\"%s\"}",
             static_cast<int64_t>(::time(nullptr)), device.address_str_to(addr_buf), device.get_rssi(),
             device.get_name().c_str());
    this->publish_state(buf);
    return true;
  }
  void dump_config() override;
};

}  // namespace ble_scanner
}  // namespace esphome

#endif
