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
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    // Escape quotes and backslashes in the device name for valid JSON
    const char *name = device.get_name().c_str();
    char escaped_name[128];
    size_t pos = 0;
    for (; *name != '\0' && pos < sizeof(escaped_name) - 2; name++) {
      if (*name == '"' || *name == '\\') {
        escaped_name[pos++] = '\\';
      }
      escaped_name[pos++] = *name;
    }
    escaped_name[pos] = '\0';

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"timestamp\":%" PRId64 ",\"address\":\"%s\",\"rssi\":%d,\"name\":\"%s\"}",
             static_cast<int64_t>(::time(nullptr)), device.address_str_to(addr_buf), device.get_rssi(), escaped_name);
    this->publish_state(buf);
    return true;
  }
  void dump_config() override;
};

}  // namespace ble_scanner
}  // namespace esphome

#endif
